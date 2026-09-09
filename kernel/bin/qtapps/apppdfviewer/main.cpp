#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QFileDialog>
#include <QScrollArea>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QListWidget>
#include <QSplitter>
#include <QEvent>
#include <QMouseEvent>
#include <QTextEdit>
#include <QInputDialog>
#include <QMenuBar>
#include <QAction>
#include <QMessageBox>
#include <QStatusBar>
#include <QScrollBar>
#include <QProgressBar>

#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>

#define APPICON_PATH            "/usr/share/gui/icons/pdf.png"

// Include MuPDF headers
extern "C" {
#include <mupdf/fitz.h>
}

// Custom Event IDs
enum CustomEventTypes 
{
    RenderFinishedEventId = QEvent::User + 101,
    ThumbnailFinishedEventId = QEvent::User + 102
};

// Selection tracking
struct SelectionState 
{
    bool is_selecting = false;
    QPoint start_pos;
    QPoint current_pos;
    QRect selection_rect;
};

// Structural container managing search metrics matches
struct SearchHit 
{
    fz_quad quad;
};

// Item structure for outline map tracking
struct OutlineItem 
{
    QString title;
    int page_number;
};

// Shared State Container
struct PdfDocument 
{
    std::mutex mtx; // Mutual exclusion lock protecting MuPDF engine handles across threads
    fz_context *ctx = nullptr;
    fz_document *doc = nullptr;
    QString current_file_path;

    int page_count = 0;
    int current_page = 0;
    float zoom_factor = 1.0f;

    enum FitMode { FitWidth, FitPage, FreeZoom };
    FitMode current_fit_mode = FitWidth;

    QString current_search_term;
    std::vector<SearchHit> search_hits;
    SelectionState selection;
    std::vector<OutlineItem> outline_items;

    ~PdfDocument() 
    {
        if(doc) fz_drop_document(ctx, doc);
        if(ctx) fz_drop_context(ctx);
    }
};

// Custom Event payloads to route asynchronously generated pixel data frames 
// back into the GUI main event thread
class RenderFinishedEvent : public QEvent 
{
public:
    QPixmap pixmap;
    int page_index;
    float zoom;

    RenderFinishedEvent(QPixmap pix, int pIdx, float zm) 
        : QEvent(static_cast<QEvent::Type>(RenderFinishedEventId)),
                 pixmap(std::move(pix)),
                 page_index(pIdx),
                 zoom(zm) {}
};

class ThumbnailFinishedEvent : public QEvent 
{
public:
    QPixmap pixmap;
    int page_index;
    ThumbnailFinishedEvent(QPixmap pix, int pIdx) 
        : QEvent(static_cast<QEvent::Type>(ThumbnailFinishedEventId)),
                 pixmap(std::move(pix)),
                 page_index(pIdx) {}
};

#if defined(FZ_VERSION_MAJOR) && (FZ_VERSION_MAJOR > 1 || (FZ_VERSION_MAJOR == 1 && FZ_VERSION_MINOR >= 24))
// New MuPDF API signature
#define MUPDF_SEARCH_TEXT(ctx, text, needle, quads, max_hits) \
        do { \
            int hit_marks[512] = {0}; \
            found_count = fz_search_stext_page(ctx, text, needle, hit_marks, quads, max_hits); \
        } while(0)
#define GET_OUTLINE_PAGE(node) ((node)->page.page)
#else
// Legacy MuPDF API signature
#define MUPDF_SEARCH_TEXT(ctx, text, needle, quads, max_hits) \
        found_count = fz_search_stext_page(ctx, text, needle, quads, max_hits)
#define GET_OUTLINE_PAGE(node) ((node)->page)
#endif

// Asynchronous Thread Pipeline Worker Class
class AsyncWorkerPipeline 
{
private:
    std::thread worker_thread;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> running{true};
    QObject* event_receiver = nullptr;

    struct Task 
    {
        enum Type { RenderPage, GenerateThumbnail } type;
        std::shared_ptr<PdfDocument> pdf;
        int page_index;
        float zoom;
    };
    std::queue<Task> task_queue;

public:
    AsyncWorkerPipeline(QObject* receiver) : event_receiver(receiver) 
    {
        worker_thread = std::thread([this]() { this->processLoop(); });
    }

    ~AsyncWorkerPipeline() 
    {
        running = false;
        cv.notify_one();

        if(worker_thread.joinable()) 
        {
            worker_thread.join();
        }
    }

    void submitRenderTask(std::shared_ptr<PdfDocument> pdf, int page, float zoom) 
    {
        std::lock_guard<std::mutex> lock(queue_mutex);

        // Evict historical render requests, but do NOT dump queued thumbnail generation threads
        std::queue<Task> filtered;

        while(!task_queue.empty()) 
        {
            if(task_queue.front().type == Task::GenerateThumbnail) filtered.push(task_queue.front());
            task_queue.pop();
        }

        task_queue = filtered;
        task_queue.push({Task::RenderPage, pdf, page, zoom});
        cv.notify_one();
    }

    void submitThumbnailTask(std::shared_ptr<PdfDocument> pdf, int page) 
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        task_queue.push({Task::GenerateThumbnail, pdf, page, 0.15f}); // Lower resolution ratio (15%)
        cv.notify_one();
    }

private:
    void parseOutlineInternal(fz_context *ctx, fz_outline *node,
                              std::vector<OutlineItem> &dest, int depth = 0) 
    {
        while(node) 
        {
            if(node->title) 
            {
                QString indentTitle = QString("%1%2").arg(QString("  ").repeated(depth)).arg(node->title);
                dest.push_back(OutlineItem{indentTitle, GET_OUTLINE_PAGE(node)});
            }

            if(node->down) parseOutlineInternal(ctx, node->down, dest, depth + 1);
            node = node->next;
        }
    }

    void processLoop() 
    {
        while(running) 
        {
            Task task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                cv.wait(lock, [this]() { return !task_queue.empty() || !running; });
                if(!running) return;
                task = task_queue.front();
                task_queue.pop();
            }

            if(task.type == Task::RenderPage) 
            {
                std::lock_guard<std::mutex> pdf_lock(task.pdf->mtx);
                if(!task.pdf->doc) continue;

                // Execute vector-to-bitmap calculations asynchronously inside worker threads
                fz_page *page = nullptr;
                fz_pixmap *pix = nullptr;
                QPixmap resultPixmap;

                try
                {
                    page = fz_load_page(task.pdf->ctx, task.pdf->doc, task.page_index);
                    fz_matrix ctm = fz_scale(task.zoom, task.zoom);
                    pix = fz_new_pixmap_from_page_number(task.pdf->ctx, task.pdf->doc, 
                                    task.page_index, ctm, fz_device_rgb(task.pdf->ctx), 0);

                    unsigned char *samples = fz_pixmap_samples(task.pdf->ctx, pix);
                    int width = fz_pixmap_width(task.pdf->ctx, pix);
                    int height = fz_pixmap_height(task.pdf->ctx, pix);

                    QImage img(samples, width, height, width * 3, QImage::Format_RGB888);
                    resultPixmap = QPixmap::fromImage(img.copy());

                    // Compute dynamic search highlights on worker background threads
                    if(!task.pdf->current_search_term.isEmpty()) 
                    {
                        fz_stext_page *text_page = fz_new_stext_page(task.pdf->ctx, 
                                                        fz_bound_page(task.pdf->ctx, page));
                        fz_device *dev = fz_new_stext_device(task.pdf->ctx, text_page, nullptr);
                        fz_run_page(task.pdf->ctx, page, dev, fz_identity, nullptr);
                        fz_close_device(task.pdf->ctx, dev);

                        constexpr int MAX_HITS = 512;
                        fz_quad hit_quads[MAX_HITS];
                        int found_count = 0;

                        MUPDF_SEARCH_TEXT(task.pdf->ctx, text_page, 
                                          task.pdf->current_search_term.toUtf8().constData(), 
                                          hit_quads, MAX_HITS);

                        QPainter painter(&resultPixmap);
                        painter.setBrush(QColor(255, 255, 0, 100));
                        painter.setPen(Qt::NoPen);

                        for(int i = 0; i < found_count; ++i) 
                        {
                            fz_quad q = hit_quads[i];
                            fz_quad trans_q = fz_transform_quad(q, ctm);

                            QPolygonF poly;
                            poly << QPointF(trans_q.ul.x, trans_q.ul.y) 
                                 << QPointF(trans_q.ur.x, trans_q.ur.y)
                                 << QPointF(trans_q.lr.x, trans_q.lr.y)
                                 << QPointF(trans_q.ll.x, trans_q.ll.y);
                            painter.drawPolygon(poly);
                        }

                        fz_drop_device(task.pdf->ctx, dev);
                        fz_drop_stext_page(task.pdf->ctx, text_page);
                    }
                } catch (...) {}

                if(pix) fz_drop_pixmap(task.pdf->ctx, pix);
                if(page) fz_drop_page(task.pdf->ctx, page);

                // Dispatch finished image frames cleanly straight back across 
                // thread barriers to the UI thread main loop
                QCoreApplication::postEvent(event_receiver, 
                        new RenderFinishedEvent(resultPixmap, task.page_index, task.zoom));
            } 
            else if(task.type == Task::GenerateThumbnail) 
            {
                std::lock_guard<std::mutex> pdf_lock(task.pdf->mtx);
                if(!task.pdf->doc) continue;

                fz_page *page = nullptr;
                fz_pixmap *pix = nullptr;
                QPixmap thumbPixmap;

                try
                {
                    page = fz_load_page(task.pdf->ctx, task.pdf->doc, task.page_index);
                    // Force render a miniature variant locked at 15% scaling
                    fz_matrix ctm = fz_scale(0.15f, 0.15f);
                    pix = fz_new_pixmap_from_page_number(task.pdf->ctx, task.pdf->doc, 
                                    task.page_index, ctm, fz_device_rgb(task.pdf->ctx), 0);

                    unsigned char *samples = fz_pixmap_samples(task.pdf->ctx, pix);
                    int width = fz_pixmap_width(task.pdf->ctx, pix);
                    int height = fz_pixmap_height(task.pdf->ctx, pix);

                    QImage img(samples, width, height, width * 3, QImage::Format_RGB888);
                    thumbPixmap = QPixmap::fromImage(img.copy());
                } catch (...) {}

                if(pix) fz_drop_pixmap(task.pdf->ctx, pix);
                if(page) fz_drop_page(task.pdf->ctx, page);

                // Post the thumbnail object to the UI main loop thread receiver
                QCoreApplication::postEvent(event_receiver, 
                                new ThumbnailFinishedEvent(thumbPixmap, task.page_index));
            }
        }
    }
};

// Selection parsing helper called directly by mouse actions
QString extractTextFromRect(PdfDocument &pdf, const QRect &screen_rect) 
{
    std::lock_guard<std::mutex> lock(pdf.mtx);
    if(!pdf.doc || screen_rect.isEmpty()) return QString();

    fz_page *page = nullptr;
    fz_stext_page *text_page = nullptr;
    fz_device *dev = nullptr;
    QString resultText;

    try
    {
        page = fz_load_page(pdf.ctx, pdf.doc, pdf.current_page);
        text_page = fz_new_stext_page(pdf.ctx, fz_bound_page(pdf.ctx, page));
        dev = fz_new_stext_device(pdf.ctx, text_page, nullptr);
        fz_run_page(pdf.ctx, page, dev, fz_identity, nullptr);
        fz_close_device(pdf.ctx, dev);

        fz_matrix ctm = fz_scale(pdf.zoom_factor, pdf.zoom_factor);
        fz_matrix inv_ctm = fz_invert_matrix(ctm);

        fz_rect search_rect = fz_make_rect(screen_rect.left(), screen_rect.top(), screen_rect.right(), screen_rect.bottom());
        fz_rect trans_rect = fz_transform_rect(search_rect, inv_ctm);

        // Enforce proper geometry ordering (MuPDF expects coordinates mapped from top-left to bottom-right)
        fz_point p_start = { std::min(trans_rect.x0, trans_rect.x1), std::min(trans_rect.y0, trans_rect.y1) };
        fz_point p_end   = { std::max(trans_rect.x0, trans_rect.x1), std::max(trans_rect.y0, trans_rect.y1) };
        
        //fz_point p_start = { trans_rect.x0, trans_rect.y0 };
        //fz_point p_end   = { trans_rect.x1, trans_rect.y1 };

        // Correct core selector endpoint naming mapped directly into MuPDF
        char *copied_text = fz_copy_selection(pdf.ctx, text_page, p_start, p_end, 0);

        if(copied_text) 
        {
            resultText = QString::fromUtf8(copied_text);
            fz_free(pdf.ctx, copied_text);
        }
    } catch (...) {}

    if(dev) fz_drop_device(pdf.ctx, dev);
    if(text_page) fz_drop_stext_page(pdf.ctx, text_page);
    if(page) fz_drop_page(pdf.ctx, page);

    return resultText.trimmed();
}


struct ResizeBridge : public QObject 
{
    std::function<bool(QObject*, QEvent*)> t;
    bool eventFilter(QObject *o, QEvent *e) override { return t ? t(o, e) : false; }
};

struct ScrollBridge : public QObject 
{
    std::function<bool(QObject*, QEvent*)> t;
    bool eventFilter(QObject *o, QEvent *e) override { return t ? t(o, e) : false; }
};

struct MouseBridge : public QObject 
{
    std::function<bool(QObject*, QEvent*)> t;
    bool eventFilter(QObject *o, QEvent *e) override { return t ? t(o, e) : false; }
};


int main(int argc, char *argv[]) 
{
    QApplication app(argc, argv);

    auto pdf = std::make_shared<PdfDocument>();
    pdf->ctx = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    fz_register_document_handlers(pdf->ctx);

    QMainWindow window;
    window.setWindowTitle("PDF Viewer");
    window.setWindowIcon(QIcon(APPICON_PATH));

    QScreen *primaryScreen = QGuiApplication::primaryScreen();

    if(primaryScreen) 
    {
        QRect screenGeometry = primaryScreen->availableGeometry();
        int screenWidth = screenGeometry.width();
        int screenHeight = screenGeometry.height();

        // Propose our desired desktop application dimensions (1200x800)
        int proposedWidth = 1200;
        int proposedHeight = 800;

        // Enforce a ceiling limit matching hardware screen limits 
        int finalWidth = std::min(proposedWidth, screenWidth - 30);
        int finalHeight = std::min(proposedHeight, screenHeight - 30);

        window.resize(finalWidth, finalHeight);
    }
    else
    {
        // Fallback hard limit envelope if screen devices fail to report properties 
        window.resize(1024, 768);
    }

    QStatusBar *statusBar = window.statusBar();
    QLabel *lblStatusMetrics = new QLabel(&window);
    lblStatusMetrics->setText("No document loaded");
    statusBar->addWidget(lblStatusMetrics, 1);

    QProgressBar *progressBar = new QProgressBar(&window);
    progressBar->setRange(0, 100);
    progressBar->setFixedWidth(150);
    progressBar->setVisible(false);
    statusBar->addPermanentWidget(progressBar);

    // =========================================================================
    // SYSTEM MENU BAR 
    // =========================================================================
    QMenuBar *menuBar = window.menuBar();

    // --- FILE MENU ---
    QMenu *menuFile = menuBar->addMenu("&File");
    QAction *actOpen = menuFile->addAction("&Open");
    actOpen->setShortcut(QKeySequence::Open);

    QAction *actClose  = menuFile->addAction("&Close");
    actClose->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));

    QAction *actSaveAs = menuFile->addAction("Save &As...");
    actSaveAs->setShortcut(QKeySequence::SaveAs);

    menuFile->addSeparator();

    QAction *actProperties = menuFile->addAction("Document &Properties...");
    actProperties->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));

    menuFile->addSeparator();

    QAction *actQuit = menuFile->addAction("E&xit");
    actQuit->setShortcut(QKeySequence::Quit);

    // --- VIEW MENU ---
    QMenu *menuView = menuBar->addMenu("&View");
    QAction *actPrev = menuView->addAction("&Previous Page");
    actPrev->setShortcut(QKeySequence(Qt::Key_Left));

    QAction *actNext = menuView->addAction("&Next Page");
    actNext->setShortcut(QKeySequence(Qt::Key_Right));

    menuView->addSeparator();

    QAction *actNextChapter = menuView->addAction("Go to Next Chapter");
    actNextChapter->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageDown));

    QAction *actPrevChapter = menuView->addAction("Go to Previous Chapter");
    actPrevChapter->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageUp));

    QAction *actFirstPage = menuView->addAction("Go to &Beginning");
    actFirstPage->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Home));

    QAction *actLastPage = menuView->addAction("Go to &End");
    actLastPage->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_End));

    menuView->addSeparator();

    QAction *actZoomIn = menuView->addAction("Zoom &In");
    actZoomIn->setShortcut(QKeySequence::ZoomIn);

    QAction *actZoomOut = menuView->addAction("Zoom &Out");
    actZoomOut->setShortcut(QKeySequence::ZoomOut);

    QAction *actFitWidth = menuView->addAction("Fit to Width");
    actFitWidth->setCheckable(true);
    actFitWidth->setChecked(true);

    QAction *actFitPage  = menuView->addAction("Fit Entire Page");
    actFitPage->setCheckable(true);

    // --- HELP MENU ---
    QMenu *menuHelp = menuBar->addMenu("&Help");
    QAction *actShortcuts = menuHelp->addAction("&Keyboard shortcuts");
    actShortcuts->setShortcut(QKeySequence(Qt::Key_F1));

    QAction *actAbout = menuHelp->addAction("&About...");

    // =========================================================================
    // TOP CONTROLS BAR WIDGET
    // =========================================================================
    QWidget *centralWidget = new QWidget(&window);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    QWidget *topBarWidget = new QWidget(nullptr);
    QHBoxLayout *controlsLayout = new QHBoxLayout(topBarWidget);

    controlsLayout->setContentsMargins(5, 5, 5, 5);
    topBarWidget->setFixedHeight(45); 

    QPushButton *btnOpen   = new QPushButton("Open PDF", topBarWidget);
    QPushButton *btnPrev   = new QPushButton("< Prev", topBarWidget);
    QPushButton *btnNext   = new QPushButton("Next >", topBarWidget);
    QPushButton *btnZoomIn  = new QPushButton("Zoom +", topBarWidget);
    QPushButton *btnZoomOut = new QPushButton("Zoom -", topBarWidget);
    QLineEdit *txtSearch   = new QLineEdit(topBarWidget);

    txtSearch->setPlaceholderText("Search for...");
    txtSearch->setFixedWidth(220);

    controlsLayout->addWidget(btnOpen);
    controlsLayout->addWidget(btnPrev);
    controlsLayout->addWidget(btnNext);
    controlsLayout->addWidget(btnZoomIn);
    controlsLayout->addWidget(btnZoomOut);
    controlsLayout->addWidget(txtSearch);
    controlsLayout->addStretch();

    mainLayout->addWidget(topBarWidget);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, centralWidget);
    QListWidget *lstOutline = new QListWidget(splitter);
    lstOutline->setMaximumWidth(180);

    lstOutline->setStyleSheet(
        "QListWidget::item { margin-left: auto; margin-right: auto; padding: 5px; text-align: center; }"
    );

    QScrollArea *scrollArea = new QScrollArea(splitter);
    QLabel *lblCanvas = new QLabel(scrollArea);

    lblCanvas->setAlignment(Qt::AlignCenter);
    lblCanvas->setMouseTracking(true);

    scrollArea->setWidget(lblCanvas);
    scrollArea->setWidgetResizable(true);

    splitter->addWidget(lstOutline);
    splitter->addWidget(scrollArea);
    mainLayout->addWidget(splitter);
    window.setCentralWidget(centralWidget);

    //Event Multiplex Filter to process custom async pipeline events
    struct AsyncUiReceiver : public QObject 
    {
        std::shared_ptr<PdfDocument> pdf;
        QLabel* canvas;
        QLabel* statusLabel;
        QListWidget* outline_view;
        QProgressBar* progress;

        protected:
        bool event(QEvent* event) override 
        {
            if(event->type() == static_cast<QEvent::Type>(RenderFinishedEventId)) 
            {
                auto* re = static_cast<RenderFinishedEvent*>(event);

                if(re->page_index == pdf->current_page && qFuzzyCompare(re->zoom, pdf->zoom_factor)) 
                {
                    canvas->setPixmap(re->pixmap);

                    int percent = static_cast<int>(pdf->zoom_factor * 100);
                    statusLabel->setText(QString("Page: %1 / %2 (%3%)")
                        .arg(pdf->current_page + 1)
                        .arg(pdf->page_count)
                        .arg(percent));

                    progress->setVisible(false);

                    // Auto-sync highlight target matching side panel records
                    for(int i = 0; i < outline_view->count(); ++i) 
                    {
                        if(outline_view->item(i)->data(Qt::UserRole).toInt() == pdf->current_page) 
                        {
                            outline_view->setCurrentRow(i);
                            break;
                        }
                    }
                }

                return true;
            }
        
            if(event->type() == static_cast<QEvent::Type>(ThumbnailFinishedEventId)) 
            {
                auto* te = static_cast<ThumbnailFinishedEvent*>(event);
                // Locate the targeted template row element placeholder and replace its icon mapping
                if(te->page_index >= 0 && te->page_index < outline_view->count()) 
                {
                    QListWidgetItem* item = outline_view->item(te->page_index);
                    item->setIcon(QIcon(te->pixmap));
                }

                return true;
            }

            return QObject::event(event);
        }
    };

    AsyncUiReceiver* uiReceiver = new AsyncUiReceiver();
    uiReceiver->pdf = pdf;
    uiReceiver->canvas = lblCanvas;
    uiReceiver->statusLabel = lblStatusMetrics;
    uiReceiver->outline_view = lstOutline;
    uiReceiver->progress = progressBar;
    window.installEventFilter(uiReceiver);

    // Instantiate Async Processing Thread
    auto asyncPipeline = std::make_unique<AsyncWorkerPipeline>(uiReceiver);

    auto updateView = [pdf, &asyncPipeline, scrollArea, progressBar]() 
    {
        if(!pdf->doc) return;

        progressBar->setMinimum(0);
        progressBar->setMaximum(0);
        progressBar->setVisible(true);

        float calculatedZoom = pdf->zoom_factor;
        {
            std::lock_guard<std::mutex> lock(pdf->mtx);

            if(pdf->current_fit_mode != PdfDocument::FreeZoom) 
            {
                fz_page *page = fz_load_page(pdf->ctx, pdf->doc, pdf->current_page);
                fz_rect rect = fz_bound_page(pdf->ctx, page);

                float pw = rect.x1 - rect.x0;
                float ph = rect.y1 - rect.y0;

                // Deduct slight scrollbar viewport padding margins
                float aw = scrollArea->viewport()->width() - 20;
                float ah = scrollArea->viewport()->height() - 20;
                //fprintf(stderr, "pw %f, ph %f, aw %f, ah %f\n", pw, ph, aw, ah);

                if(pw > 0 && ph > 0 && aw > 0 && ah > 0) 
                {
                    if(pdf->current_fit_mode == PdfDocument::FitWidth) 
                    {
                        calculatedZoom = aw / pw;
                    }
                    else
                    {
                        calculatedZoom = std::min(aw / pw, ah / ph);
                    }

                    pdf->zoom_factor = calculatedZoom;
                }

                fz_drop_page(pdf->ctx, page);
            }
        }

        asyncPipeline->submitRenderTask(pdf, pdf->current_page, pdf->zoom_factor);
    };

    // =========================================================================
    // INITIAL STATE MANAGEMENT
    // =========================================================================
    // Elements that depend on an active document context
    std::vector<QAction*> docDependentActions = 
    {
        actClose, actSaveAs, actProperties, actPrev, actNext, 
        actZoomIn, actZoomOut, actFitWidth, actFitPage,
        actNextChapter, actPrevChapter, actFirstPage, actLastPage
    };

    // Buttons that depend on an active document context
    std::vector<QPushButton*> docDependentButtons = 
    {
        btnPrev, btnNext, btnZoomIn, btnZoomOut
    };

    for(QAction *action : docDependentActions)   action->setEnabled(false);
    for(QPushButton *button : docDependentButtons) button->setEnabled(false);

    // =========================================================================
    // File Menu Actions
    // =========================================================================
    // Exit application
    QObject::connect(actQuit, &QAction::triggered, &app, &QApplication::quit);

    // Close document
    auto triggerFileClose = [pdf, &window, lstOutline, lblCanvas, lblStatusMetrics, docDependentActions, docDependentButtons]() 
    {
        if(!pdf->doc) return;

        {
            std::lock_guard<std::mutex> lock(pdf->mtx);
            fz_drop_document(pdf->ctx, pdf->doc);
            pdf->doc = nullptr;
            pdf->current_file_path.clear();
            pdf->page_count = 0;
            pdf->current_page = 0;
            pdf->outline_items.clear();
        }

        lblCanvas->clear();
        lstOutline->clear();
        window.setWindowTitle("PDF Viewer");
        lblStatusMetrics->setText("No document loaded");

        for(QAction *action : docDependentActions)   action->setEnabled(false);
        for(QPushButton *button : docDependentButtons) button->setEnabled(false);
    };

    QObject::connect(actClose, &QAction::triggered, centralWidget, triggerFileClose);

    // Shared logic between "Open PDF" button and menu item
    auto loadDocumentFile = [pdf, &window, lstOutline, docDependentActions, docDependentButtons, asyncPipeline = asyncPipeline.get(), updateView](const QString &filePath) 
    {
        if(filePath.isEmpty()) return;

        bool outlineLoaded = false;
        std::vector<OutlineItem> localItems;

        {
            std::lock_guard<std::mutex> lock(pdf->mtx);

            if(pdf->doc) 
            {
                fz_drop_document(pdf->ctx, pdf->doc);
                pdf->doc = nullptr;
            }

            try
            {
                pdf->doc = fz_open_document(pdf->ctx, filePath.toUtf8().constData());
                while(fz_needs_password(pdf->ctx, pdf->doc)) 
                {
                    bool ok = false;
                    QString password = QInputDialog::getText(&window, "Encrypted File Detected", 
                                                            "Please enter the PDF password:", 
                                                            QLineEdit::Password, "", &ok);
                    if(!ok) { fz_drop_document(pdf->ctx, pdf->doc); pdf->doc = nullptr; return; }
                    if(fz_authenticate_password(pdf->ctx, pdf->doc, password.toUtf8().constData())) break; 
                }

                pdf->page_count = fz_count_pages(pdf->ctx, pdf->doc);
                pdf->current_page = 0;
                pdf->current_search_term.clear();
                pdf->current_file_path = filePath;

                fz_outline *outline = fz_load_outline(pdf->ctx, pdf->doc);

                if(outline) 
                {
                    outlineLoaded = true;
                    auto const parseOutline = [](auto& self, fz_context *ctx, fz_outline *node, std::vector<OutlineItem> &dest, int depth) -> void 
                    {
                        while(node) 
                        {
                            QString indentTitle = QString("%1%2").arg(QString("  ").repeated(depth)).arg(node->title);
                            dest.push_back(OutlineItem{indentTitle, GET_OUTLINE_PAGE(node)});

                            if(node->down) self(self, ctx, node->down, dest, depth + 1);
                            node = node->next;
                        }
                    };

                    parseOutline(parseOutline, pdf->ctx, outline, localItems, 0);
                    fz_drop_outline(pdf->ctx, outline);
                }
            } catch (...) {
                fprintf(stderr, "Error while opening document\n");
                return;
            }
        }

        for(QAction *action : docDependentActions)   action->setEnabled(true);
        for(QPushButton *button : docDependentButtons) button->setEnabled(true);

        QFileInfo info(filePath);
        window.setWindowTitle(QString("%1 - PDF Viewer").arg(info.fileName()));
        lstOutline->clear();

        if(outlineLoaded) 
        {
            // Document contains outline (i.e. table of contents)
            lstOutline->setViewMode(QListView::ListMode);
            lstOutline->setIconSize(QSize(0, 0));
            pdf->outline_items = localItems;

            for(const auto &item : localItems) 
            {
                QListWidgetItem *listItem = new QListWidgetItem(item.title, lstOutline);
                listItem->setData(Qt::UserRole, item.page_number);
            }
        }
        else
        {
            // No outline, show page thumbnails
            lstOutline->setViewMode(QListView::IconMode);
            lstOutline->setFlow(QListView::LeftToRight);
            lstOutline->setItemAlignment(Qt::AlignHCenter);
            lstOutline->setGridSize(QSize(160, 170));
            lstOutline->setWrapping(true);
            lstOutline->setResizeMode(QListView::Adjust);
            lstOutline->setIconSize(QSize(120, 160));
            lstOutline->setMovement(QListView::Static);
            lstOutline->setSpacing(10);
            lstOutline->setStyleSheet("");

            pdf->outline_items.clear();

            for(int i = 0; i < pdf->page_count; ++i) 
            {
                QListWidgetItem *thumbItem = new QListWidgetItem(QString("Page %1").arg(i + 1), lstOutline);
                thumbItem->setData(Qt::UserRole, i);
                thumbItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);

                // Attach a blank image placeholder until the background thread finishes rendering
                thumbItem->setIcon(QIcon()); 

                // Submit thumbnail request to our background queue
                asyncPipeline->submitThumbnailTask(pdf, i);
            }
        }

        updateView();
    };

    auto triggerFileOpen = [&window, loadDocumentFile]() 
    {
        QString path = QFileDialog::getOpenFileName(&window, "Open Document", "", "PDF (*.pdf)");
        loadDocumentFile(path);
    };

    QObject::connect(btnOpen, &QPushButton::clicked, centralWidget, triggerFileOpen);
    QObject::connect(actOpen, &QAction::triggered, centralWidget, triggerFileOpen);

    QObject::connect(actSaveAs, &QAction::triggered, [centralWidget, pdf, &window]() 
    {
        if(pdf->current_file_path.isEmpty()) return;
        QString targetPath = QFileDialog::getSaveFileName(&window, "Save Copy As", "", "PDF Files (*.pdf)");
        if(!targetPath.isEmpty()) 
        {
            QFile::copy(pdf->current_file_path, targetPath);
        }
    });

    QObject::connect(actProperties, &QAction::triggered, centralWidget, [pdf, &window]() 
    {
        if(!pdf->doc) 
        {
            QMessageBox::warning(&window, "Properties", "No open document.");
            return;
        }

        std::lock_guard<std::mutex> lock(pdf->mtx);

        // Output buffers for MuPDF string allocations
        char titleBuf[256] = {0};
        char authorBuf[256] = {0};
        char formatBuf[256] = {0};
        QString metaText;

        metaText.append(QString("<b>File page:</b> %1<br>").arg(pdf->current_file_path));

        if(fz_lookup_metadata(pdf->ctx, pdf->doc, "info:Title", titleBuf, sizeof(titleBuf)) > 0 && titleBuf[0] != '\0')
            metaText.append(QString("<b>Title:</b> %1<br>").arg(QString::fromUtf8(titleBuf)));
        else 
            metaText.append("<b>Title:</b> Unknown / Unspecified<br>");

        if(fz_lookup_metadata(pdf->ctx, pdf->doc, "info:Author", authorBuf, sizeof(authorBuf)) > 0 && authorBuf[0] != '\0')
            metaText.append(QString("<b>Author:</b> %1<br>").arg(QString::fromUtf8(authorBuf)));
        else
            metaText.append("<b>Author:</b> Unknown / Unspecified<br>");

        if(fz_lookup_metadata(pdf->ctx, pdf->doc, "format", formatBuf, sizeof(formatBuf)) > 0 && formatBuf[0] != '\0')
            metaText.append(QString("<b>Format Engine:</b> %1<br>").arg(QString::fromUtf8(formatBuf)));
        else
            metaText.append("<b>Format Engine:</b> PDF Vector Document<br>");

        metaText.append(QString("<b>Total Pages:</b> %1<br>").arg(pdf->page_count));
        metaText.append(QString("<b>Security Encryption:</b> %1<br>").arg(fz_needs_password(pdf->ctx, pdf->doc) ? "Password Encrypted File" : "Unencrypted / Cleartext"));

        // Render via a Modal Dialog frame
        QDialog *dlg = new QDialog(&window);
        dlg->setWindowTitle("Document Properties");
        dlg->setMinimumWidth(400);
        QVBoxLayout *lay = new QVBoxLayout(dlg);
        
        QLabel *lblMeta = new QLabel(dlg);
        lblMeta->setTextFormat(Qt::RichText);
        lblMeta->setText(metaText);
        
        QPushButton *btnClose = new QPushButton("Close", dlg);
        QObject::connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);

        lay->addWidget(lblMeta);
        lay->addWidget(btnClose);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->exec();
    });

    // =========================================================================
    // View Menu Actions
    // =========================================================================
    auto triggerPrevPage = [pdf, updateView]() 
    {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(pdf->mtx);
            if(pdf->doc && pdf->current_page > 0) { pdf->current_page--; changed = true; }
        }

        if(changed) updateView();
    };

    QObject::connect(btnPrev, &QPushButton::clicked, centralWidget, triggerPrevPage);
    QObject::connect(actPrev, &QAction::triggered, centralWidget, triggerPrevPage);

    auto triggerNextPage = [pdf, updateView]() 
    {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(pdf->mtx);
            if(pdf->doc && pdf->current_page < (pdf->page_count - 1)) { pdf->current_page++; changed = true; }
        }

        if(changed) updateView();
    };

    QObject::connect(btnNext, &QPushButton::clicked, centralWidget, triggerNextPage);
    QObject::connect(actNext, &QAction::triggered, centralWidget, triggerNextPage);

    QObject::connect(actFirstPage, &QAction::triggered, centralWidget, [pdf, updateView]() 
    {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(pdf->mtx);
            if(pdf->doc && pdf->current_page != 0) { pdf->current_page = 0; changed = true; }
        }

        if(changed) updateView();
    });

    QObject::connect(actLastPage, &QAction::triggered, centralWidget, [pdf, updateView]() 
    {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(pdf->mtx);
            if(pdf->doc && pdf->current_page != (pdf->page_count - 1)) 
            { 
                pdf->current_page = pdf->page_count - 1; changed = true; 
            }
        }

        if(changed) updateView();
    });

    auto triggerZoomIn = [pdf, actFitWidth, actFitPage, updateView]() 
    {
        {
            std::lock_guard<std::mutex> lock(pdf->mtx);
            pdf->current_fit_mode = PdfDocument::FreeZoom;
            if(pdf->doc && pdf->zoom_factor < 4.0f) { pdf->zoom_factor += 0.2f; }
        }

        actFitWidth->setChecked(false);
        actFitPage->setChecked(false);
        updateView();
    };

    QObject::connect(btnZoomIn, &QPushButton::clicked, centralWidget, triggerZoomIn);
    QObject::connect(actZoomIn, &QAction::triggered, centralWidget, triggerZoomIn);

    auto triggerZoomOut = [pdf, actFitWidth, actFitPage, updateView]() 
    {
        {
            std::lock_guard<std::mutex> lock(pdf->mtx);
            pdf->current_fit_mode = PdfDocument::FreeZoom;
            if(pdf->doc && pdf->zoom_factor > 0.4f) { pdf->zoom_factor -= 0.2f; }
        }

        actFitWidth->setChecked(false);
        actFitPage->setChecked(false);
        updateView();
    };

    QObject::connect(btnZoomOut, &QPushButton::clicked, centralWidget, triggerZoomOut);
    QObject::connect(actZoomOut, &QAction::triggered, centralWidget, triggerZoomOut);

    QObject::connect(actFitWidth, &QAction::triggered, centralWidget, [pdf, actFitPage, updateView](bool checked) 
    {
        if(checked) 
        {
            std::lock_guard<std::mutex> lock(pdf->mtx);
            pdf->current_fit_mode = PdfDocument::FitWidth;
            actFitPage->setChecked(false);
        }
        else
        {
            std::lock_guard<std::mutex> lock(pdf->mtx);
            pdf->current_fit_mode = PdfDocument::FreeZoom;
        }

        updateView();
    });

    QObject::connect(actFitPage, &QAction::triggered, centralWidget, [pdf, actFitWidth, updateView](bool checked) 
    {
        if(checked) 
        {
            std::lock_guard<std::mutex> lock(pdf->mtx);
            pdf->current_fit_mode = PdfDocument::FitPage;
            actFitWidth->setChecked(false);
        }
        else
        {
            std::lock_guard<std::mutex> lock(pdf->mtx);
            pdf->current_fit_mode = PdfDocument::FreeZoom;
        }

        updateView();
    });

    auto triggerNextChapter = [pdf, lstOutline, updateView]() 
    {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(pdf->mtx);
            for(const auto &item : pdf->outline_items) 
            {
                if(item.page_number > pdf->current_page) 
                {
                    pdf->current_page = item.page_number;
                    changed = true;
                    break;
                }
            }
        }

        if(changed) updateView();
    };

    QObject::connect(actNextChapter, &QAction::triggered, centralWidget, triggerNextChapter);

    auto triggerPrevChapter = [pdf, lstOutline, updateView]() 
    {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(pdf->mtx);
            int targetPage = 0;
            for(auto it = pdf->outline_items.rbegin(); it != pdf->outline_items.rend(); ++it) 
            {
                if(it->page_number < pdf->current_page) 
                {
                    targetPage = it->page_number;
                    changed = true;
                    break;
                }
            }

            if(changed) pdf->current_page = targetPage;
        }

        if(changed) updateView();
    };

    QObject::connect(actPrevChapter, &QAction::triggered, centralWidget, triggerPrevChapter);

    // =========================================================================
    // Help Menu Actions
    // =========================================================================
    QObject::connect(actShortcuts, &QAction::triggered, centralWidget, [&window]() 
    {
        QMessageBox msgBox(&window);
        msgBox.setWindowTitle("Keyboard Shortcuts");
        msgBox.setTextFormat(Qt::MarkdownText);
    
        msgBox.setText(
            "| Shortcut | Action |\n"
            "| :--- | :--- |\n"
            "| **Ctrl + +** | Zoom in |\n"
            "| **Ctrl + -** | Zoom out |\n"
            "| **Right arrow** | Goto next page |\n"
            "| **Left arrow** | Goto previous page |\n"
            "| **Ctrl + PgUp** | Goto previous chapter |\n"
            "| **Ctrl + PgDn** | Goto next chapter |\n"
            "| **Ctrl + Home** | Goto first page |\n"
            "| **Ctrl + End** | Goto last page |\n"
            "| **Ctrl + E** | Fit page |\n"
            "| **Ctrl + I** | Document properties |\n"
            "| **Ctrl + O** | Open file |\n"
            "| **Ctrl + Q** | Quit |\n"
            "| **Ctrl + W** | Close document |\n"
            "| **Ctrl + Shift + W** | Fit window width |\n"
            "| **Ctrl + Shift + S** | Save as |\n"
        );
    
        msgBox.setIcon(QMessageBox::Information);
        msgBox.exec();
    });

    QObject::connect(actAbout, &QAction::triggered, centralWidget, [&window]() 
    {
        QMessageBox msgBox(&window);
        msgBox.setWindowTitle("About PDF Viewer");
        msgBox.setTextFormat(Qt::MarkdownText);

        msgBox.setText(
            "PDF viewer built using **Qt 6**.\n\n"
            "Tailored to run seamlessly across traditional GNU/Linux host environments, LaylaOS, and "
            "custom bare-metal hobby operating system platforms."
        );

        msgBox.setIcon(QMessageBox::Information);
        msgBox.exec();
    });


    // --- Selection Engine Filter Bindings ---
    MouseBridge *mouseFilter = new MouseBridge();
    mouseFilter->setParent(lblCanvas);
    lblCanvas->installEventFilter(mouseFilter);

    mouseFilter->t = [pdf, updateView, &window, lblCanvas](QObject *, QEvent *event) -> bool 
    {
        if(!pdf->doc) return false;

        if(event->type() == QEvent::MouseButtonPress) 
        {
            auto *me = static_cast<QMouseEvent*>(event);

            if(me->button() == Qt::LeftButton) 
            {
                pdf->selection.is_selecting = true;
                pdf->selection.start_pos = me->position().toPoint();
                pdf->selection.selection_rect = QRect();
                return true;
            }
        }
        else if(event->type() == QEvent::MouseMove && pdf->selection.is_selecting) 
        {
            auto *me = static_cast<QMouseEvent*>(event);
            pdf->selection.current_pos = me->position().toPoint();
            pdf->selection.selection_rect = QRect(pdf->selection.start_pos, pdf->selection.current_pos).normalized();

            QPixmap currentPixmap = lblCanvas->pixmap();

            if(!currentPixmap.isNull()) 
            {
                QPixmap tempPixmap = currentPixmap;
                QPainter painter(&tempPixmap);
                painter.setBrush(QColor(0, 120, 215, 50));
                painter.setPen(QPen(QColor(0, 120, 215, 200), 1, Qt::DashLine));
                painter.drawRect(pdf->selection.selection_rect);
                lblCanvas->setPixmap(tempPixmap);
            }

            return true;
        }
        else if(event->type() == QEvent::MouseButtonRelease) 
        {
            auto *me = static_cast<QMouseEvent*>(event);
            if(me->button() == Qt::LeftButton && pdf->selection.is_selecting) 
            {
                pdf->selection.is_selecting = false;
                QString copied = extractTextFromRect(*pdf, pdf->selection.selection_rect);
                pdf->selection.selection_rect = QRect();
                updateView();

                if(!copied.isEmpty()) 
                {
                    QDialog *dlg = new QDialog(&window);
                    dlg->setWindowTitle("Extracted Text");
                    dlg->resize(450, 200);

                    QVBoxLayout *layout = new QVBoxLayout(dlg);
                    QTextEdit *edit = new QTextEdit(dlg);
                    edit->setPlainText(copied);
                    layout->addWidget(edit);
                    dlg->setAttribute(Qt::WA_DeleteOnClose);
                    dlg->show();
                }

                return true;
            }
        }

        return false;
    };

    // Other Control Connections
    QObject::connect(txtSearch, &QLineEdit::textChanged, centralWidget, [pdf, updateView](const QString &text) 
    {
        {
            std::lock_guard<std::mutex> lock(pdf->mtx);
            pdf->current_search_term = text;
        }

        updateView();
    });

    QObject::connect(lstOutline, &QListWidget::itemClicked, centralWidget, [pdf, updateView](QListWidgetItem *item) 
    {
        {
            QVariant pageData = item->data(Qt::UserRole);

            if(pageData.isValid()) 
            {
                std::lock_guard<std::mutex> lock(pdf->mtx);

                int targetPage = pageData.toInt();
                if(targetPage >= 0 && targetPage < pdf->page_count) 
                {
                    pdf->current_page = targetPage;
                }
            }
        }

        updateView();
    });

    QObject::connect(lstOutline, &QListWidget::currentItemChanged, centralWidget, [pdf, updateView](QListWidgetItem *current, QListWidgetItem *) 
    {
        if(!current) return;
        QVariant data = current->data(Qt::UserRole);

        if(data.isValid()) 
        {
            bool changed = false;
            {
                std::lock_guard<std::mutex> lock(pdf->mtx);
                int targetPage = data.toInt();
                if(pdf->current_page != targetPage) 
                {
                    pdf->current_page = targetPage;
                    changed = true;
                }
            }
            if(changed) updateView();
        }
    });

    // Window resize
    ResizeBridge *resizeFilter = new ResizeBridge();
    resizeFilter->setParent(scrollArea);
    scrollArea->viewport()->installEventFilter(resizeFilter);

    resizeFilter->t = [pdf, updateView](QObject *, QEvent *event) -> bool
    {
        if(event->type() == QEvent::Resize && pdf->current_fit_mode != PdfDocument::FreeZoom) 
        {
            updateView();
        }

        return false;
    };

    // =========================================================================
    // KEYBOARD SHORTCUTS FOR VIEW SCALING
    // =========================================================================
    QAction *actKeyFitWidth = new QAction(&window);
    actKeyFitWidth->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));
    window.addAction(actKeyFitWidth);
    QObject::connect(actKeyFitWidth, &QAction::triggered, centralWidget, [pdf, actFitWidth, actFitPage, updateView]() 
    {
        {
            std::lock_guard<std::mutex> lock(pdf->mtx);
            pdf->current_fit_mode = PdfDocument::FitWidth;
        }
        actFitWidth->setChecked(true);
        actFitPage->setChecked(false);
        updateView();
    });

    QAction *actKeyFitPage = new QAction(&window);
    actKeyFitPage->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    window.addAction(actKeyFitPage);
    QObject::connect(actKeyFitPage, &QAction::triggered, centralWidget, [pdf, actFitWidth, actFitPage, updateView]() 
    {
        {
            std::lock_guard<std::mutex> lock(pdf->mtx);
            pdf->current_fit_mode = PdfDocument::FitPage;
        }
        actFitWidth->setChecked(false);
        actFitPage->setChecked(true);
        updateView();
    });

    // Page Up / Page Down Keyboard Listeners
    QAction *actKeyPageUp = new QAction(&window);
    actKeyPageUp->setShortcut(QKeySequence(Qt::Key_PageUp));
    window.addAction(actKeyPageUp);
    QObject::connect(actKeyPageUp, &QAction::triggered, centralWidget, triggerPrevPage);

    QAction *actKeyPageDown = new QAction(&window);
    actKeyPageDown->setShortcut(QKeySequence(Qt::Key_PageDown));
    window.addAction(actKeyPageDown);
    QObject::connect(actKeyPageDown, &QAction::triggered, centralWidget, triggerNextPage);

    // Continuous page flow
    ScrollBridge *scrollFilter = new ScrollBridge();
    scrollFilter->setParent(scrollArea);
    scrollArea->verticalScrollBar()->installEventFilter(scrollFilter);

    // Guard tracking rollover flags to prevent scroll oscillation
    auto scrollBounceLock = std::make_shared<bool>(false);

    scrollFilter->t = [pdf, updateView, scrollArea, scrollBounceLock](QObject *, QEvent *event) -> bool {
        if(event->type() == QEvent::Wheel && pdf->doc && !(*scrollBounceLock)) 
        {
            QScrollBar *bar = scrollArea->verticalScrollBar();
            int val = bar->value();
            int max = bar->maximum();

            bool pageChanged = false;
            int targetPage = -1;

            std::unique_lock<std::mutex> lock(pdf->mtx, std::try_to_lock);
            if(!lock.owns_lock()) return false;

            if(val >= max && pdf->current_page < (pdf->page_count - 1)) 
            {
                targetPage = pdf->current_page + 1;
                pageChanged = true;
            }
            else if(val <= 0) 
            {
                targetPage = pdf->current_page - 1;
                pageChanged = true;
            }

            if(pageChanged && targetPage != -1) 
            {
                *scrollBounceLock = true; // Block incoming events BEFORE executing changes
                pdf->current_page = targetPage;
                
                lock.unlock();
                updateView();
                
                QCoreApplication::processEvents();

                if(val >= max) 
                {
                    bar->setValue(2);
                }
                else
                {
                    bar->setValue(bar->maximum() - 2);
                }
                
                *scrollBounceLock = false; // Re-enable scroll processing
                return true;
            }
        }

        return false;
    };

    lstOutline->setFocus(Qt::OtherFocusReason);
    window.show();

    // =========================================================================
    // Commandline parameters
    // =========================================================================
    QStringList args = QApplication::arguments();

    if(args.size() > 1) 
    {
        QString cmdFilePath = args.at(1);
        loadDocumentFile(cmdFilePath);
    }

    return app.exec();
}

