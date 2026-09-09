#include <QGuiApplication>
#include <QTabWidget>
#include <QScrollBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QPalette>
#include <QStyleFactory>
#include <QTimer>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QTextCharFormat>
#include <QPrinter>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QTextBlock>
#include <QKeyEvent>
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QCloseEvent>

#include <atomic>
#include <vector>
#include <map>

#include "editor.hpp"

#define APPICON_PATH            "/usr/share/gui/icons/edit.png"


Editor::Editor(QWidget *parent) 
        : QMainWindow(parent), isDarkMode(false), 
          stopSpellCheck(false), spellCheckPending(false), hunspellEngine(nullptr)
{
    hunspellEngine = Hunspell_create("/usr/share/hunspell/en_US.aff", "/usr/share/hunspell/en_US.dic");

    setupUI();
    setupAutoSave();
    loadRecentFiles();
    updateRecentFilesMenu();
    startSpellCheckWorker();

    // Check if any arguments were loaded during window boot.
    // If the workspace mapping remains empty, create a single blank tab.
    QTimer::singleShot(0, this, [this]()
    {
        if(tabsMap.empty())
        {
            addNewTab();
        }
    });

    initializeLanguageRepository();
}

Editor::~Editor()
{
    stopSpellCheck = true;

    if(spellCheckThread.joinable())
    {
        spellCheckThread.join();
    }

    if(hunspellEngine)
    {
        Hunspell_destroy(hunspellEngine);
        hunspellEngine = nullptr;
    }
}


void Editor::initializeLanguageRepository()
{
    // Define shared style tokens once
    keywordFormat.setForeground(QColor(42, 130, 218));
    keywordFormat.setFontWeight(QFont::Bold);
    stringFormat.setForeground(QColor(165, 42, 42));
    commentFormat.setForeground(QColor(0, 128, 0));
    commentFormat.setFontItalic(true);
    numberFormat.setForeground(QColor(255, 140, 0));
    tagFormat.setForeground(QColor(139, 0, 139));
    tagFormat.setFontWeight(QFont::Bold);

    // Helper lambda to easily inject keywords into a specific extension bucket
    auto addKeywordsToLang = [this](const QString &lang, const QStringList &keywords, bool caseInsensitive = false)
    {
        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;

        if(caseInsensitive)
        {
            options = QRegularExpression::CaseInsensitiveOption;
        }

        for(const QString &keyword : keywords)
        {
            languageRulesRepository[lang].push_back({QRegularExpression(keyword, options), keywordFormat});
        }
    };

    // --- C / C++ PROFILE ---
    // https://cppreference.com/c/keyword
    QStringList cKeywords = { "\\basm\\b", "\\balignas\\b", "\\balignof\\b", "\\bauto\\b",
                              "\\bbool\\b", "\\bbreak\\b", "\\bcase\\b",
                              "\\bchar\\b", "\\bconst\\b", "\\bconstexpr\\b",
                              "\\bcontinue\\b", "\\bdefault\\b", "\\bdo\\b",
                              "\\bdouble\\b", "\\belse\\b", "\\benum\\b",
                              "\\bextern\\b", "\\bfalse\\b", "\\bfloat\\b",
                              "\\bfor\\b", "\\bgoto\\b", "\\bif\\b", "\\binline\\b",
                              "\\bint\\b", "\\blong\\b", "\\bnullptr\\b",
                              "\\bregister\\b", "\\brestrict\\b", "\\breturn\\b",
                              "\\bshort\\b", "\\bsigned\\b", "\\bsizeof\\b",
                              "\\bstatic\\b", "\\bassert\\b", "\\bstruct\\b",
                              "\\bswitch\\b", "\\bthread_local\\b", "\\btrue\\b",
                              "\\btypedef\\b", "\\btypeof\\b", "\\btypeof_unqual\\b",
                              "\\bunion\\b", "\\bunsigned\\b", "\\bvoid\\b",
                              "\\bvolatile\\b", "\\bwhile\\b", "\\bAlignas\\b",
                              "\\bAlignof\\b", "\\b_Atomic\\b", "\\bBitInt\\b",
                              "\\bBool\\b", "\\bComplex\\b", "\\bDecimal128\\b",
                              "\\bDecimal32\\b", "\\bDecimal64\\b", "\\bGeneric\\b",
                              "\\b_Imaginary\\b", "\\bNoreturn\\b", "\\b_Thread_local\\b",
                            };

    addKeywordsToLang("cpp", cKeywords);

    // Add Preprocessor Directives rule (matches # followed by keywords or words)
    languageRulesRepository["cpp"].push_back({QRegularExpression("#\\s*\\w+"), keywordFormat});

    // Single-line comment rule
    languageRulesRepository["cpp"].push_back({QRegularExpression("//[^\n]*"), commentFormat});

    // Multiline comment rule
    // This instructs the regex engine to treat newlines as regular string matching assets
    languageRulesRepository["cpp"].push_back({
        QRegularExpression("/\\*.*?\\*/", QRegularExpression::DotMatchesEverythingOption), 
        commentFormat
    });

    // Link headers/C files to use the exact same memory map vector pointer
    languageRulesRepository["c"] = languageRulesRepository["cpp"];
    languageRulesRepository["h"] = languageRulesRepository["cpp"];
    languageRulesRepository["hpp"] = languageRulesRepository["cpp"];

    // --- PYTHON PROFILE ---
    // https://docs.python.org/3/reference/lexical_analysis.html#names-identifiers-and-keywords
    QStringList pyKeywords = { "\\bFalse\\b", "\\bNone\\b", "\\bTrue\\b", "\\band\\b",
                               "\\bas\\b", "\\bassert\\b", "\\basync\\b", "\\bawait\\b",
                               "\\bbreak\\b", "\\bclass\\b", "\\bcontinue\\b", "\\bdef\\b",
                               "\\bdel\\b", "\\belif\\b", "\\belse\\b", "\\bexcept\\b",
                               "\\bfinally\\b", "\\bfor\\b", "\\bfrom\\b", "\\bglobal\\b",
                               "\\bif\\b", "\\bimport\\b", "\\bin\\b", "\\bis\\b",
                               "\\blambda\\b", "\\bnonlocal\\b", "\\bnot\\b", "\\bor\\b",
                               "\\bpass\\b", "\\braise\\b", "\\breturn\\b", "\\btry\\b",
                               "\\bwhile\\b", "\\bwith\\b", "\\byield\\b",
                               "\\bmatch\\b", "\\bcase\\b", "\\btype\\b",
                             };
    addKeywordsToLang("py", pyKeywords);
    languageRulesRepository["py"].push_back({QRegularExpression("#[^\n]*"), commentFormat});

    // --- JAVA PROFILE ---
    // https://docs.oracle.com/javase/tutorial/java/nutsandbolts/_keywords.html
    QStringList javaKeywords = { "\\babstract\\b", "\\bassert\\b", "\\bboolean\\b",
                                 "\\bbreak\\b", "\\bbyte\\b", "\\bcase\\b", "\\bcatch\\b",
                                 "\\bchar\\b", "\\bclass\\b", "\\bconst\\b", "\\bcontinue\\b",
                                 "\\bdefault\\b", "\\bdo\\b", "\\bdouble\\b", "\\belse\\b",
                                 "\\benum\\b", "\\bextends\\b", "\\bfinal\\b", "\\bfinally\\b",
                                 "\\bfloat\\b", "\\bfor\\b", "\\bgoto\\b", "\\bif\\b",
                                 "\\bimplements\\b", "\\bimport\\b", "\\binstanceof\\b",
                                 "\\bint\\b", "\\binterface\\b", "\\blong\\b", "\\bnative\\b",
                                 "\\bnew\\b", "\\bpackage\\b", "\\bprivate\\b", "\\bprotected\\b",
                                 "\\bpublic\\b", "\\breturn\\b", "\\bshort\\b", "\\bstatic\\b",
                                 "\\bstrictfp\\b", "\\bsuper\\b", "\\bswitch\\b",
                                 "\\bsynchronized\\b", "\\bthis\\b", "\\bthrow\\b",
                                 "\\bthrows\\b", "\\btransient\\b", "\\btry\\b",
                                 "\\bvoid\\b", "\\bvolatile\\b", "\\bwhile\\b",
                                 "\\bconst\\b", "\\bgoto\\b",
                                 "\\btrue\\b", "\\bfalse\\b", "\\bnull\\b",
                               };
    addKeywordsToLang("java", javaKeywords);
    languageRulesRepository["java"].push_back({QRegularExpression("//[^\n]*"), commentFormat});

    // Multiline comment rule
    // This instructs the regex engine to treat newlines as regular string matching assets
    languageRulesRepository["java"].push_back({
        QRegularExpression("/\\*.*?\\*/", QRegularExpression::DotMatchesEverythingOption), 
        commentFormat
    });

    // --- JAVASCRIPT PROFILE ---
    // https://www.w3schools.com/js/js_reserved.asp
    QStringList jsKeywords = { "\\babstract\\b", "\\bboolean\\b", "\\bcatch\\b",
                               "\\bcontinue\\b", "\\bd\\b", "\\beval\\b", "\\bfinal\\b",
                               "\\bfunction\\b", "\\bint\\b", "\\bnative\\b", "\\bprivate\\b",
                               "\\bshort\\b", "\\bsynchronized\\b", "\\btransient\\b",
                               "\\busing\\b", "\\bwhile\\b", "\\barguments\\b", "\\bbreak\\b",
                               "\\bchar\\b", "\\bdebugger\\b", "\\bdouble\\b", "\\bexport\\b",
                               "\\bfinally\\b", "\\bgoto\\b", "\\bimport\\b", "\\binterface\\b",
                               "\\bnew\\b", "\\bprotected\\b", "\\bstatic\\b", "\\bthis\\b",
                               "\\btrue\\b", "\\bvar\\b", "\\bwith\\b", "\\basync\\b",
                               "\\bbyte\\b", "\\bclass\\b", "\\bdefault\\b", "\\belse\\b",
                               "\\bextends\\b", "\\bfloat\\b", "\\bif\\b", "\\bin\\b",
                               "\\blet\\b", "\\bnull\\b", "\\bpublic\\b", "\\bsuper\\b",
                               "\\bthrow\\b", "\\btry\\b", "\\bvoid\\b", "\\byield\\b",
                               "\\bawait\\b", "\\bcase\\b", "\\bconst\\b", "\\bdelete\\b",
                               "\\benum\\b", "\\bfalse\\b", "\\bfor\\b", "\\bimplements\\b",
                               "\\binstanceof\\b", "\\blong\\b", "\\bpackage\\b",
                               "\\breturn\\b", "\\bswitch\\b", "\\bthrows\\b",
                               "\\btypeof\\b", "\\bvolatile\\b",
                             };
    addKeywordsToLang("js", jsKeywords);
    languageRulesRepository["js"].push_back({QRegularExpression("//[^\n]*"), commentFormat});

    // Multiline comment rule
    // This instructs the regex engine to treat newlines as regular string matching assets
    languageRulesRepository["js"].push_back({
        QRegularExpression("/\\*.*?\\*/", QRegularExpression::DotMatchesEverythingOption), 
        commentFormat
    });

    languageRulesRepository["ts"] = languageRulesRepository["js"]; // Link TypeScript

    // --- SQL PROFILE ---
    // https://www.w3schools.com/sql/sql_ref_keywords.asp
    QStringList sqlKeywords = { "\\bADD\\b", "\\bADD CONSTRAINT\\b", "\\bALL\\b", "\\bALTER\\b",
                                "\\bAND\\b", "\\bANY\\b", "\\bAS\\b", "\\bASC\\b",
                                "\\bBACKUP\\b", "\\bBETWEEN\\b", "\\bCASE\\b",
                                "\\bCHECK\\b", "\\bCOLUMN\\b", "\\bCONSTRAINT\\b", "\\bCREATE\\b",
                                "\\bCREATE OR REPLACE VIEW\\b",
                                "\\bDATABASE\\b", "\\bDEFAULT\\b",
                                "\\bDELETE\\b", "\\bDESC\\b", "\\bDISTINCT\\b", "\\bDROP\\b",
                                "\\bEXEC\\b", "\\bEXISTS\\b", "\\bFOREIGN KEY\\b", "\\bFROM\\b",
                                "\\bFULL OUTER JOIN\\b", "\\bGROUP BY\\b", "\\bHAVING\\b",
                                "\\bIN\\b", "\\bINDEX\\b", "\\bINNER JOIN\\b",
                                "\\bINSERT INTO\\b", "\\bIS NULL\\b", "\\bIS NOT NULL\\b",
                                "\\bJOIN\\b", "\\bLEFT JOIN\\b", "\\bLIKE\\b", "\\bLIMIT\\b",
                                "\\bNOT\\b", "\\bNOT NULL\\b", "\\bOR\\b", "\\bORDER BY\\b",
                                "\\bOUTER JOIN\\b", "\\bPRIMARY KEY\\b", "\\bPROCEDURE\\b",
                                "\\bRIGHT JOIN\\b", "\\bROWNUM\\b", "\\bSELECT\\b",
                                "\\bSELECT DISTINCT\\b", "\\bSELECT INTO\\b", "\\bSELECT TOP\\b",
                                "\\bSET\\b", "\\bTABLE\\b", "\\bTOP\\b", "\\bTRUNCATE TABLE\\b",
                                "\\bUNION\\b", "\\bUNIQUE\\b", "\\bUPDATE\\b", "\\bVALUES\\b",
                                "\\bVIEW\\b", "\\bWHERE\\b",
                              };
    addKeywordsToLang("sql", sqlKeywords);
    languageRulesRepository["sql"].push_back({QRegularExpression("--[^\n]*"), commentFormat});

    // --- HTML / XML PROFILE ---
    // https://www.w3schools.com/tags/ref_byfunc.asp
    languageRulesRepository["html"].push_back({QRegularExpression("<[^>]*>"), tagFormat});
    languageRulesRepository["html"].push_back({QRegularExpression("<!--.*?-->"), commentFormat});
    languageRulesRepository["xml"] = languageRulesRepository["html"];

    // --- PASCAL PROFILE ---
    // https://wiki.freepascal.org/Reserved_words
    QStringList pasKeywords = { // turbo pascal
                                "\\band\\b", "\\barray\\b", "\\basm\\b", "\\bbegin\\b",
                                "\\bbreak\\b", "\\bcase\\b", "\\bconst\\b", "\\bconstructor\\b",
                                "\\bcontinue\\b", "\\bdestructor\\b", "\\bdiv\\b", "\\bdo\\b",
                                "\\bdownto\\b", "\\belse\\b", "\\bend\\b", "\\bfalse\\b",
                                "\\bfile\\b", "\\bfor\\b", "\\bfunction\\b", "\\bgoto\\b",
                                "\\bif\\b", "\\bimplementation\\b", "\\bin\\b", "\\binline\\b",
                                "\\binterface\\b", "\\blabel\\b", "\\bmod\\b", "\\bnil\\b",
                                "\\bnot\\b", "\\bobject\\b", "\\bof\\b", "\\bon\\b",
                                "\\boperator\\b", "\\bor\\b", "\\bpacked\\b", "\\bprocedure\\b",
                                "\\bprogram\\b", "\\brecord\\b", "\\brepeat\\b", "\\bset\\b",
                                "\\bshl\\b", "\\bshr\\b", "\\bstring\\b", "\\bthen\\b", "\\bto\\b",
                                "\\btrue\\b", "\\btype\\b", "\\bunit\\b", "\\buntil\\b",
                                "\\buses\\b", "\\bvar\\b", "\\bwhile\\b", "\\bwith\\b", "\\bxor\\b",
                                // object pascal
                                "\\bas\\b", "\\bclass\\b", "\\bconstref\\b", "\\bdispose\\b",
                                "\\bexcept\\b", "\\bexit\\b", "\\bexports\\b", "\\bfinalization\\b",
                                "\\bfinally\\b", "\\binherited\\b", "\\binitialization\\b",
                                "\\bis\\b", "\\blibrary\\b", "\\bnew\\b", "\\bon\\b", "\\bout\\b",
                                "\\bproperty\\b", "\\braise\\b", "\\bself\\b", "\\bthreadvar\\b",
                                "\\btry\\b",
                              };
    addKeywordsToLang("pas", pasKeywords, true);
    languageRulesRepository["pas"].push_back({QRegularExpression("//[^\n]*"), commentFormat});

    languageRulesRepository["pas"].push_back({
        QRegularExpression("\\(\\*.*?\\*\\)", QRegularExpression::DotMatchesEverythingOption),
        commentFormat
    });

    // --- ASSEMBLY PROFILE ---
    // https://docs.oracle.com/cd/E19120-01/open.solaris/817-5477/enmzx/index.html
    QStringList asmKeywords = { // data transfer instructions
                                "\\bbswap\\b", "\\bcbw\\b", "\\bcdq\\b", "\\bcdqe\\b",
                                "\\bcmova\\b", "\\bcmovae\\b", "\\bcmovb\\b", "\\bcmovbe\\b",
                                "\\bcmovc\\b", "\\bcmove\\b", "\\bcmovg\\b", "\\bcmovge\\b",
                                "\\bcmovl\\b", "\\bcmovle\\b", "\\bcmovna\\b", "\\bcmovnae\\b",
                                "\\bcmovnb\\b", "\\bcmovnbe\\b", "\\bcmovnc\\b", "\\bcmovne\\b",
                                "\\bcmovng\\b", "\\bcmovnge\\b", "\\bcmovnl\\b", "\\bcmovnle\\b",
                                "\\bcmovno\\b", "\\bcmovnp\\b", "\\bcmovns\\b", "\\bcmovnz\\b",
                                "\\bcmovo\\b", "\\bcmovp\\b", "\\bcmovpe\\b", "\\bcmovpo\\b",
                                "\\bcmovs\\b", "\\bcmovz\\b", "\\bcmpxchg\\b", "\\bcmpxchg8b\\b",
                                "\\bcqo\\b", "\\bcwd\\b", "\\bcwde\\b", "\\bmov\\b",
                                "\\bmovabs\\b", "\\bmovsx\\b", "\\bmovzx\\b", "\\bpop\\b",
                                "\\bpopa\\b", "\\bpopad\\b", "\\bpush\\b", "\\bpusha\\b",
                                "\\bpushad\\b", "\\bxadd\\b", "\\bxchg\\b",
                                // binary arithmetic instructions
                                "\\badc\\b", "\\badd\\b", "\\bcmp\\b", "\\bdec\\b", "\\bdiv\\b",
                                "\\bidiv\\b", "\\bimul\\b", "\\binc\\b", "\\bmul\\b", "\\bneg\\b",
                                "\\bsbb\\b", "\\bsub\\b",
                                // decimal arithmetic instructions
                                "\\baaa\\b", "\\baad\\b", "\\baam\\b", "\\baas\\b",
                                "\\bdaa\\b", "\\bdas\\b",
                                // logical instructions
                                "\\band\\b", "\\bnot\\b", "\\bor\\b", "\\bxor\\b",
                                // shift and rotate instructions
                                "\\brcl\\b", "\\brcr\\b", "\\brol\\b", "\\bror\\b",
                                "\\bsal\\b", "\\bsar\\b", "\\bshl\\b", "\\bshld\\b",
                                "\\bshr\\b", "\\bshrd\\b",
                                // bit and byte instructions
                                "\\bbsf\\b", "\\bbsr\\b", "\\bbt\\b", "\\bbtc\\b", "\\bbtr\\b",
                                "\\bbts\\b", "\\bseta\\b", "\\bsetae\\b", "\\bsetb\\b", "\\bsetbe\\b",
                                "\\bsetc\\b", "\\bsete\\b", "\\bsetg\\b", "\\bsetge\\b", "\\bsetl\\b",
                                "\\bsetle\\b", "\\bsetna\\b", "\\bsetnae\\b", "\\bsetnb\\b",
                                "\\bsetnbe\\b", "\\bsetnc\\b", "\\bsetne\\b", "\\bsetng\\b",
                                "\\bsetnge\\b", "\\bsetnl\\b", "\\bsetnle\\b", "\\bsetno\\b",
                                "\\bsetnp\\b", "\\bsetns\\b", "\\bsetnz\\b", "\\bseto\\b",
                                "\\bsetp\\b", "\\bsetpe\\b", "\\bsetpo\\b", "\\bsets\\b",
                                "\\bsetz\\b", "\\btest\\b",
                                // control transfer instructions
                                "\\bbound\\b", "\\bcall\\b", "\\benter\\b", "\\bint\\b", "\\binto\\b",
                                "\\biret\\b", "\\bja\\b", "\\bjae\\b", "\\bjb\\b", "\\bjbe\\b",
                                "\\bjc\\b", "\\bjcxz\\b", "\\bje\\b", "\\bjecxz\\b", "\\bjg\\b",
                                "\\bjge\\b", "\\bjl\\b", "\\bjle\\b", "\\bjmp\\b", "\\bjnae\\b",
                                "\\bjnb\\b", "\\bjnbe\\b", "\\bjnc\\b", "\\bjne\\b", "\\bjng\\b",
                                "\\bjnge\\b", "\\bjnl\\b", "\\bjnle\\b", "\\bjno\\b", "\\bjnp\\b",
                                "\\bjns\\b", "\\bjnz\\b", "\\bjo\\b", "\\bjp\\b", "\\bjpe\\b",
                                "\\bjpo\\b", "\\bjs\\b", "\\bjz\\b", "\\bcall\\b", "\\bleave\\b",
                                "\\bloop\\b", "\\bloope\\b", "\\bloopne\\b", "\\bloopnz\\b",
                                "\\bloopz\\b", "\\bret\\b",
                                // string instructions
                                "\\bcmps\\b", "\\bcmpsb\\b", "\\bcmpsd\\b", "\\bcmpsw\\b",
                                "\\blods\\b", "\\blodsb\\b", "\\blodsd\\b", "\\blodsw\\b",
                                "\\bmovs\\b", "\\bmovsb\\b", "\\bmovsd\\b", "\\bmovsw\\b",
                                "\\brep\\b", "\\brepne\\b", "\\brepnz\\b", "\\brepe\\b",
                                "\\brepz\\b", "\\bscas\\b", "\\bscasb\\b", "\\bscasd\\b",
                                "\\bscasw\\b", "\\bstos\\b", "\\bstosb\\b", "\\bstosd\\b", "\\bstosw\\b",
                                // I/O instructions
                                "\\bin\\b", "\\bins\\b", "\\binsb\\b", "\\binsd\\b", "\\binsw\\b",
                                "\\bout\\b", "\\bouts\\b", "\\boutsb\\b", "\\boutsd\\b", "\\boutsw\\b",
                                // EFLAGS instructions
                                "\\bclc\\b", "\\bcld\\b", "\\bcli\\b", "\\bcmc\\b", "\\blahf\\b",
                                "\\bpopf\\b", "\\bpopfl\\b", "\\bpushf\\b", "\\bpushfl\\b",
                                "\\bsahf\\b", "\\bstc\\b", "\\bstd\\b", "\\bsti\\b",
                                // segment register instructions
                                "\\blds\\b", "\\bles\\b", "\\blfs\\b", "\\blgs\\b", "\\blss\\b",
                                // misc instructions
                                "\\bcpuid\\b", "\\blea\\b", "\\bnop\\b", "\\bud2\\b",
                                "\\bxlat\\b", "\\bxlatb\\b",
                                // data transfer instructions (FP)
                                "\\bfbld\\b", "\\bfbstp\\b", "\\bfcmovb\\b", "\\bfcmovbe\\b",
                                "\\bfcmove\\b", "\\bfcmovnb\\b", "\\bfcmovnbe\\b", "\\bfcmovne\\b",
                                "\\bfcmovnu\\b", "\\bfcmou\\b", "\\bfild\\b", "\\bfist\\b",
                                "\\bfistp\\b", "\\bfld\\b", "\\bfst\\b", "\\bfstp\\b", "\\bfxch\\b",
                                // basic arithmetic instructions (FP)
                                "\\bfabs\\b", "\\bfadd\\b", "\\bfaddp\\b", "\\bfchs\\b", "\\bfdiv\\b",
                                "\\bfdivp\\b", "\\bfdivr\\b", "\\bfdivrp\\b", "\\bfiadd\\b",
                                "\\bfidiv\\b", "\\bfidivr\\b", "\\bfimul\\b", "\\bfisub\\b",
                                "\\bfisubr\\b", "\\bfmul\\b", "\\bfmulp\\b", "\\bfprem\\b",
                                "\\bfprem1\\b", "\\bfrndint\\b", "\\bfscale\\b", "\\bsqrt\\b",
                                "\\bfsub\\b", "\\bfsubp\\b", "\\bfsubr\\b", "\\bfsubrp\\b",
                                "\\bfxtract\\b",
                                // comparison instructions (FP)
                                "\\bfcom\\b", "\\bfcomi\\b", "\\bfcomip\\b", "\\bfcomp\\b",
                                "\\bfcompp\\b", "\\bficom\\b", "\\bficomp\\b", "\\bftst\\b",
                                "\\bfucom\\b", "\\bfucomi\\b", "\\bfucomip\\b", "\\bfucomp\\b",
                                "\\bfucompp\\b", "\\bfxam\\b",
                                // transcendental instructions (FP)
                                "\\bf2xm1\\b", "\\bfcos\\b", "\\bfpatan\\b", "\\bfptan\\b",
                                "\\bfsin\\b", "\\bfsincos\\b", "\\bfyl2x\\b", "\\bfyl2xp1\\b",
                                // load constants instructions (FP)
                                "\\bfld1\\b", "\\bfldl2e\\b", "\\bfldl2t\\b", "\\bfldlg2\\b",
                                "\\bfldln2\\b", "\\bfldpi\\b", "\\bfldz\\b",
                                // control instructions (FP)
                                "\\bfclex\\b", "\\bfdecstp\\b", "\\bffree\\b", "\\bfincstp\\b",
                                "\\bfinit\\b", "\\bfldcw\\b", "\\bfldenv\\b", "\\bfnclex\\b",
                                "\\bfninit\\b", "\\bfnop\\b", "\\bfnsave\\b", "\\bfnstcw\\b",
                                "\\bfnstenv\\b", "\\bfnstsw\\b", "\\bfrstor\\b", "\\bfsave\\b",
                                "\\bfstcw\\b", "\\bfstenv\\b", "\\bfstsw\\b", "\\bfwait\\b",
                                "\\bwait\\b",
                                // SIMD state management instructions
                                "\\bfxrstor\\b", "\\bfxsave\\b",
                                // data transfer instructions (MMX)
                                "\\bmovd\\b", "\\bmovq\\b",
                                // conversion instructions (MMX)
                                "\\bpackssdw\\b", "\\bpacksswb\\b", "\\bpackuswb\\b",
                                "\\bpunpckhbw\\b", "\\bpunpckhdq\\b", "\\bpunpckhwd\\b",
                                "\\bpunpcklbw\\b", "\\bpunpckldq\\b", "\\bpunpcklwd\\b",
                                // packed arithmetic instructions (MMX)
                                "\\bpaddb\\b", "\\bpaddd\\b", "\\bpaddsb\\b", "\\bpaddsw\\b",
                                "\\bpaddusb\\b", "\\bpaddusw\\b", "\\bpaddw\\b", "\\bpmaddwd\\b",
                                "\\bpmulhw\\b", "\\bpmullw\\b", "\\bpsubb\\b", "\\bpsubd\\b",
                                "\\bpsubsb\\b", "\\bpsubsw\\b", "\\bpsubusb\\b", "\\bpsubusw\\b",
                                "\\bpsubw\\b",
                                // comparison instructions (MMX)
                                "\\bpcmpeqb\\b", "\\bpcmpeqd\\b", "\\bpcmpeqw\\b",
                                "\\bpcmpgtb\\b", "\\bpcmpgtd\\b", "\\bpcmpgtw\\b",
                                // logical instructions (MMX)
                                "\\bpand\\b", "\\bpandn\\b", "\\bpor\\b", "\\bpxor\\b",
                                // shift and rotate instructions (MMX)
                                "\\bpslld\\b", "\\bpsllq\\b", "\\bpsllw\\b", "\\bpsrad\\b",
                                "\\bpsraw\\b", "\\bpsrld\\b", "\\bpsrlq\\b", "\\bpsrlw\\b",
                                // state management instructions (MMX)
                                "\\bemms\\b",
                                // data transfer instructions (SSE)
                                "\\bmovaps\\b", "\\bmovhlps\\b", "\\bmovhps\\b", "\\bmovlhps\\b",
                                "\\bmovlps\\b", "\\bmovmskps\\b", "\\bmovss\\b", "\\bmovups\\b",
                                // packed arithmetic instructions (SSE)
                                "\\baddps\\b", "\\baddss\\b", "\\bdivps\\b", "\\bdivss\\b",
                                "\\bmaxps\\b", "\\bmaxss\\b", "\\bminps\\b", "\\bminss\\b",
                                "\\bmulps\\b", "\\bmulss\\b", "\\brcpps\\b", "\\brcpss\\b",
                                "\\brsqrtps\\b", "\\brsqrtss\\b", "\\bsqrtps\\b", "\\bsqrtss\\b",
                                "\\bsubps\\b", "\\bsubss\\b",
                                // comparison instructions (SSE)
                                "\\bcmpps\\b", "\\bcmpss\\b", "\\bcomiss\\b", "\\bucomiss\\b",
                                // logical instructions (SSE)
                                "\\bandnps\\b", "\\bandps\\b", "\\borps\\b", "\\bxorps\\b",
                                // shuffle and unpack instructions (SSE)
                                "\\bshufps\\b", "\\bunpckhps\\b", "\\bunpcklps\\b",
                                // conversion instructions (SSE)
                                "\\bcvtpi2ps\\b", "\\bcvtps2pi\\b", "\\bcvtsi2ss\\b",
                                "\\bcvtss2si\\b", "\\bcvttps2pi\\b", "\\bcvttss2si\\b",
                                // MXCSR state management instructions (SSE)
                                "\\bldmxcsr\\b", "\\bstmxcsr\\b",
                                // 64-bit SIMD integer instructions (SSE)
                                "\\bpavgb\\b", "\\bpavgw\\b", "\\bpextrw\\b", "\\bpinsrw\\b",
                                "\\bpmaxsw\\b", "\\bpmaxub\\b", "\\bpminsw\\b", "\\bpminub\\b",
                                "\\bpmovmskb\\b", "\\bpmulhuw\\b", "\\bpsadbw\\b", "\\bpshufw\\b",
                                // misc instructions (SSE)
                                "\\baskmovq\\b", "\\bmovntps\\b", "\\bmovntq\\b", "\\bprefetchnta\\b",
                                "\\bprefetcht0\\b", "\\bprefetcht1\\b", "\\bprefetcht2\\b",
                                "\\bsfence\\b",
                                // SSE2 data movement instructions
                                "\\bmovapd\\b", "\\bmovhpd\\b", "\\bmovlpd\\b", "\\bmovmskpd\\b",
                                "\\bmovsd\\b", "\\bmovupd\\b",
                                // SSE2 packed arithmetic instructions
                                "\\baddpd\\b", "\\baddsd\\b", "\\bdivpd\\b", "\\bdivsd\\b",
                                "\\bmaxpd\\b", "\\bmaxsd\\b", "\\bminpd\\b", "\\bminsd\\b",
                                "\\bmulpd\\b", "\\bmulsd\\b", "\\bsqrtpd\\b", "\\bsqrtsd\\b",
                                "\\bsubpd\\b", "\\bsubsd\\b",
                                // SSE2 logical instructions
                                "\\bandnpd\\b", "\\bandpd\\b", "\\borpd\\b", "\\bxorpd\\b",
                                // SSE2 compare instructions
                                "\\bcmppd\\b", "\\bcmpsd\\b", "\\bcomisd\\b", "\\bucomisd\\b",
                                // SSE2 shuffle and unpack instructions
                                "\\bshufpd\\b", "\\bunpckhpd\\b", "\\bunpcklpd\\b",
                                // SSE2 conversion instructions
                                "\\bcvtdq2pd\\b", "\\bcvtpd2dq\\b", "\\bcvtpd2pi\\b",
                                "\\bcvtpd2ps\\b", "\\bcvtpi2pd\\b", "\\bcvtps2pd\\b",
                                "\\bcvtsd2si\\b", "\\bcvtsd2ss\\b", "\\bcvtsi2sd\\b",
                                "\\bcvtss2sd\\b", "\\bcvttpd2dq\\b", "\\bcvttpd2pi\\b",
                                "\\bcvttsd2si\\b",
                                // SSE2 packed single precision FP instructions
                                "\\bcvtdq2ps\\b", "\\bcvtps2dq\\b", "\\bcvttps2dq\\b",
                                // SSE2 128-bit SIMD integer instructions
                                "\\bmovdq2q\\b", "\\bmovdqa\\b", "\\bmovdqu\\b", "\\bmovq2dq\\b",
                                "\\bpaddq\\b", "\\bpmuludq\\b", "\\bpshufd\\b", "\\bpshufhw\\b",
                                "\\bpshuflw\\b", "\\bpslldq\\b", "\\bpsrldq\\b", "\\bpsubq\\b",
                                "\\bpunpckhqdq\\b", "\\bpunpcklqdq\\b",
                                // SSE2 misc instructions
                                "\\bclflush\\b", "\\blfence\\b", "\\bmaskmovdqu\\b",
                                "\\bmfence\\b", "\\bmovntdq\\b", "\\bmovnti\\b",
                                "\\bmovntpd\\b", "\\bpause\\b",
                                // OS support instructions
                                "\\barpl\\b", "\\bclts\\b", "\\bhlt\\b", "\\binvd\\b",
                                "\\binvlpg\\b", "\\blar\\b", "\\blgdt\\b", "\\blidt\\b",
                                "\\blldt\\b", "\\blmsw\\b", "\\block\\b", "\\blsl\\b",
                                "\\bltr\\b", "\\brdmsr\\b", "\\brdpmc\\b", "\\brdtsc\\b",
                                "\\brsm\\b", "\\bsgdt\\b", "\\bsidt\\b", "\\bsldt\\b",
                                "\\bsmsw\\b", "\\bstr\\b", "\\bsysenter\\b", "\\bsysexit\\b",
                                "\\bverr\\b", "\\bverw\\b", "\\bwbinvd\\b", "\\bwrmsr\\b",
                              };
    addKeywordsToLang("asm", asmKeywords, true);

    // Assembly Directives rule (matches a dot followed by any alphanumeric word, e.g., .global, .text)
    languageRulesRepository["asm"].push_back({
        QRegularExpression("\\.\\w+", QRegularExpression::CaseInsensitiveOption), 
        keywordFormat
    });

    languageRulesRepository["asm"].push_back({QRegularExpression("#[^\n]*"), commentFormat});
    languageRulesRepository["asm"].push_back({QRegularExpression(";[^\n]*"), commentFormat});
    languageRulesRepository["s"] = languageRulesRepository["asm"];

    // Add common baseline string/number rules to all languages
    for(auto& [lang, rules] : languageRulesRepository)
    {
        rules.push_back({QRegularExpression("\".*?\""), stringFormat});
        rules.push_back({QRegularExpression("'.*?'"), stringFormat});

        // Match C-Style Hex Numbers (e.g., 0x1A2B, 0XFF, 0x0)
        // Matches 0x or 0X followed by any combination of numbers and letters A-F
        rules.push_back({QRegularExpression("\\b0[xX][0-9a-fA-F]+\\b"), numberFormat});

        // Match Assembly/Pascal-Style Hex Numbers (e.g., $FF, $1a)
        // Matches a literal dollar sign followed by numbers and letters A-F
        rules.push_back({QRegularExpression("\\$[0-9a-fA-F]+\\b"), numberFormat});

        // Decimal number format
        rules.push_back({QRegularExpression("\\b\\d+\\b"), numberFormat});
    }
}


QString Editor::detectLanguageKey(const QString &filePath)
{
    QString ext = QFileInfo(filePath).suffix().toLower();

    if(languageRulesRepository.find(ext) != languageRulesRepository.end())
    {
        return ext;
    }

    return "txt"; // Default fallback (plain text)
}


void Editor::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    if(mainSplitter && fileTreeView)
    {
        QList<int> currentSizes = mainSplitter->sizes();

        if(!currentSizes.isEmpty() && currentSizes[0] == 0)
        {
            mainSplitter->setSizes(QList<int>({0, this->width()}));
            return;
        }

        int totalWidth = this->width();
        
        // Calculate the responsive 20% and 80% pixel widths
        int sidebarWidth = static_cast<int>(totalWidth * 0.20);
        int editorWidth = totalWidth - sidebarWidth;

        // Apply the dynamic layout values directly to the splitter
        mainSplitter->setSizes(QList<int>({sidebarWidth, editorWidth}));
    }
}


// Event Filter for auto-indentation matching previous line's spaces
bool Editor::eventFilter(QObject *obj, QEvent *event)
{
    QWidget *viewportWidget = qobject_cast<QWidget*>(obj);

    // Intercept minimap layout components using custom property keys
    if(viewportWidget && viewportWidget->parent() && 
       viewportWidget->parent()->property("isMinimapViewport").toBool())
    {
        QTextEdit *minimap = qobject_cast<QTextEdit*>(viewportWidget->parent());
        QTextEdit *mainEditor = minimap->property("associatedMainEditor").value<QTextEdit*>();

        if(minimap && mainEditor)
        {
            // Render the view boundary overlay frame indicator block
            if(event->type() == QEvent::Paint)
            {
                // Allow the base text layout structure components to draw underneath first
                viewportWidget->removeEventFilter(this);
                QCoreApplication::sendEvent(viewportWidget, event);
                viewportWidget->installEventFilter(this);

                if(mainEditor->toPlainText().isEmpty() || mainEditor->viewport()->height() <= 0) return true;

                QPainter painter(viewportWidget);

                int mainScrollBarMax = mainEditor->verticalScrollBar()->maximum();
                int mainVisiblePage = mainEditor->verticalScrollBar()->pageStep();
                int totalEstimatedHeight = mainScrollBarMax + mainVisiblePage;

                if(totalEstimatedHeight <= 0) return true;

                // Calculate box height and position based on scrollbar steps rather than text geometry
                double visibleRatio = static_cast<double>(mainVisiblePage) / totalEstimatedHeight;
                int boxHeight = std::max(15, static_cast<int>(minimap->viewport()->height() * visibleRatio));
    
                double scrollRatio = 0.0;

                if(mainScrollBarMax > 0)
                {
                    scrollRatio = static_cast<double>(mainEditor->verticalScrollBar()->value()) / mainScrollBarMax;
                }

                int boxY = static_cast<int>((minimap->viewport()->height() - boxHeight) * scrollRatio);

                // Draw a semi-transparent indicator frame highlight mask block layer
                painter.fillRect(QRect(0, boxY, minimap->viewport()->width(), boxHeight), QColor(0, 120, 215, 40));
                painter.setPen(QColor(0, 120, 215, 150));
                painter.drawRect(QRect(0, boxY, minimap->viewport()->width() - 1, boxHeight - 1));
                
                return true; 
            }

            // Handle viewport clicks to navigate the document line coordinates
            if(event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove)
            {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                if(mouseEvent->buttons() & Qt::LeftButton)
                {
                    double clickPercent = static_cast<double>(mouseEvent->position().y()) / minimap->viewport()->height();
                    int targetScrollValue = static_cast<int>(clickPercent * mainEditor->verticalScrollBar()->maximum());
                    
                    mainEditor->verticalScrollBar()->setValue(targetScrollValue);
                    return true;
                }
            }
        }
    }

    if(event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        if(keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
        {
            QTextEdit *editor = qobject_cast<QTextEdit*>(obj);

            if(editor && !editor->property("isMinimapViewport").toBool())
            {
                QTextCursor cursor = editor->textCursor();
                QString currentLine = cursor.block().text();

                QRegularExpression regex("^(\\s*)");
                QRegularExpressionMatch match = regex.match(currentLine);

                if(match.hasMatch())
                {
                    QString indentation = match.captured(1);
                    cursor.insertText("\n" + indentation);
                    editor->setTextCursor(cursor);
                    return true; 
                }
            }
        }
    }

    return QMainWindow::eventFilter(obj, event);
}


void Editor::setupUI()
{
    // Base workspace arrangement layout using an interactive splitter frame
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(mainSplitter);
    setWindowTitle("Text Editor");
    setWindowIcon(QIcon(APPICON_PATH));

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

        resize(finalWidth, finalHeight);
    }
    else
    {
        // Fallback hard limit envelope if screen devices fail to report properties 
        resize(1024, 768);
    }

    // Configure left interactive file explorer navigation sidebar panel
    fileTreeView = new QTreeView(mainSplitter);
    fileSystemModel = new QFileSystemModel(this);
    fileSystemModel->setRootPath(QDir::homePath());

    fileTreeView->setModel(fileSystemModel);
    fileTreeView->setRootIndex(fileSystemModel->index(QDir::homePath()));
    fileTreeView->setHeaderHidden(true);

    // Hide file detail column grids (Size, Type, Date Modified) for compact view
    for(int i = 1; i < 4; ++i) fileTreeView->hideColumn(i);

    // Double click file browser routing logic
    QObject::connect(fileTreeView, &QTreeView::doubleClicked, this, [this](const QModelIndex &index)
    {
        QString filePath = fileSystemModel->filePath(index);
        if(!fileSystemModel->isDir(index))
        {
            openFile(filePath);
        }
    });

    // Configure right editing canvas panel
    tabWidget = new QTabWidget(mainSplitter);
    tabWidget->setTabsClosable(true);

    // Add layouts inside splitter view
    mainSplitter->addWidget(fileTreeView);
    mainSplitter->addWidget(tabWidget);
    mainSplitter->setStretchFactor(0, 1); // 20% sidebar area
    mainSplitter->setStretchFactor(1, 4); // 80% work canvas area
    //mainSplitter->setSizes(QList<int>({size().width() * 0.2, size().width() * 0.8}));

    QObject::connect(tabWidget, &QTabWidget::tabCloseRequested, this, [this](int index) { closeTab(index); });
    QObject::connect(tabWidget, &QTabWidget::currentChanged, this, [this](int) { updateStatusBar(); });

    // Set up the status bar
    QStatusBar *statusBarRef = statusBar();
    statusLabel = new QLabel("Words: 0 | Characters: 0 | Tabs: 0", this);
    statusBarRef->addPermanentWidget(statusLabel);

    cursorPositionLabel = new QLabel("Line 1, Col 1", this);
    statusBar()->insertWidget(0, cursorPositionLabel);

    // Set up the main menu
    QMenu *fileMenu = menuBar()->addMenu("&File");
    QMenu *editMenu = menuBar()->addMenu("&Edit");
    QMenu *viewMenu = menuBar()->addMenu("&View");
    QMenu *helpMenu = menuBar()->addMenu("&Help");

    // File menu actions
    QAction *newTabAction = fileMenu->addAction("&New Tab");
    newTabAction->setShortcut(QKeySequence::New);

    QAction *openAction = fileMenu->addAction("&Open...");
    openAction->setShortcut(QKeySequence::Open);
    fileMenu->addSeparator();

    QAction *saveAction = fileMenu->addAction("&Save");
    saveAction->setShortcut(QKeySequence::Save);

    QAction *saveAsAction = fileMenu->addAction("Save &As...");
    saveAsAction->setShortcut(QKeySequence::SaveAs);

    QAction *exportPdfAction = fileMenu->addAction("Export to &PDF...");
    exportPdfAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    fileMenu->addSeparator();

    recentFilesMenu = fileMenu->addMenu("Open &Recent");
    fileMenu->addSeparator();

    QAction *closeTabAction = fileMenu->addAction("&Close Tab");
    closeTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));

    QAction *exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);

    // Edit menu actions
    QAction *undoAction = editMenu->addAction("&Undo");
    undoAction->setShortcut(QKeySequence::Undo);

    QAction *redoAction = editMenu->addAction("&Redo");
    redoAction->setShortcut(QKeySequence::Redo);

    editMenu->addSeparator();

    QAction *cutAction = editMenu->addAction("Cu&t");
    cutAction->setShortcut(QKeySequence::Cut);

    QAction *copyAction = editMenu->addAction("&Copy");
    copyAction->setShortcut(QKeySequence::Copy);

    QAction *pasteAction = editMenu->addAction("&Paste");
    pasteAction->setShortcut(QKeySequence::Paste);

    editMenu->addSeparator();

    QAction *findAction = editMenu->addAction("&Find and Replace...");
    findAction->setShortcut(QKeySequence::Find);

    // View menu actions
    QAction *wrapAction = viewMenu->addAction("Enable &Line Wrapping");
    wrapAction->setCheckable(true);
    wrapAction->setChecked(true);

    QAction *themeAction = viewMenu->addAction("Toggle &Dark Mode");
    themeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));

    QAction *toggleSidebarAction = viewMenu->addAction("Toggle &Sidebar");
    toggleSidebarAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));

    // Help menu actions
    QAction *shortcutsAction = helpMenu->addAction("Keyboard &Shortcuts");
    QAction *aboutAction = helpMenu->addAction("&About...");

    // Action connections
    QObject::connect(toggleSidebarAction, &QAction::triggered, this, [this]() {
        toggleSidebar();
    });

    QObject::connect(wrapAction, &QAction::triggered, this, [this, wrapAction](bool checked)
    {
        for(auto const& [index, bundle] : tabsMap)
        {
            if(bundle.mainEditor)
            {
                bundle.mainEditor->setLineWrapMode(checked ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
            }
        }
    });

    QObject::connect(closeTabAction, &QAction::triggered, this, [this]()
    {
        closeTab(tabWidget->currentIndex());
    });

    QObject::connect(newTabAction, &QAction::triggered, this, [this]() { addNewTab(); });

    QObject::connect(openAction, &QAction::triggered, this, [this]()
    {
        openFile(QFileDialog::getOpenFileName(this, "Open file"));
    });

    QObject::connect(saveAction, &QAction::triggered, this, [this]() { saveFile(); });
    QObject::connect(saveAsAction, &QAction::triggered, this, [this]() { saveCurrentFile(); });
    QObject::connect(exportPdfAction, &QAction::triggered, this, [this]() { exportCurrentToPdf(); });
    QObject::connect(exitAction, &QAction::triggered, this, [this]() { close(); });

    QObject::connect(undoAction, &QAction::triggered, this, [this]()
    {
        if (QTextEdit* edit = currentTextEdit()) edit->undo();
    });

    QObject::connect(redoAction, &QAction::triggered, this, [this]()
    {
        if (QTextEdit* edit = currentTextEdit()) edit->redo();
    });

    QObject::connect(cutAction, &QAction::triggered, this, [this]()
    {
        if (QTextEdit* edit = currentTextEdit()) edit->cut();
    });

    QObject::connect(copyAction, &QAction::triggered, this, [this]()
    {
        if (QTextEdit* edit = currentTextEdit()) edit->copy();
    });

    QObject::connect(pasteAction, &QAction::triggered, this, [this]()
    {
        if (QTextEdit* edit = currentTextEdit()) edit->paste();
    });

    QObject::connect(findAction, &QAction::triggered, this, [this]() { showFindReplaceDialog(); });

    QObject::connect(themeAction, &QAction::triggered, this, [this]() { toggleDarkMode(); });


    QObject::connect(shortcutsAction, &QAction::triggered, this, [this]()
    {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Keyboard Shortcuts");
        msgBox.setTextFormat(Qt::MarkdownText);
    
        msgBox.setText(
            "| Shortcut | Action |\n"
            "| :--- | :--- |\n"
            "| **Ctrl + B** | Toggle sidebar file explorer |\n"
            "| **Ctrl + C** | Copy selection |\n"
            "| **Ctrl + F** | Open the Find & Replace dialog |\n"
            "| **Ctrl + N** | New tab |\n"
            "| **Ctrl + O** | Open file |\n"
            "| **Ctrl + P** | Export document to PDF |\n"
            "| **Ctrl + Q** | Exit the editor |\n"
            "| **Ctrl + S** | Save file |\n"
            "| **Ctrl + Shift + S** | Save file as... |\n"
            "| **Ctrl + T** | Toggle dark mode theme |\n"
            "| **Ctrl + W** | Close current tab |\n"
            "| **Ctrl + V** | Paste from clipboard |\n"
            "| **Ctrl + Z** | Undo last action |\n"
            "| **Ctrl + Shift + Z** | Redo last action |\n"
        );
    
        msgBox.setIcon(QMessageBox::Information);
        msgBox.exec();
    });

    QObject::connect(aboutAction, &QAction::triggered, this, [this]()
    {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("About Text Editor");
        msgBox.setTextFormat(Qt::MarkdownText);

        msgBox.setText(
            "Text editor built using **Qt 6**.\n\n"
            "Tailored to run seamlessly across traditional GNU/Linux host environments, LaylaOS, and "
            "custom bare-metal hobby operating system platforms."
        );

        msgBox.setIcon(QMessageBox::Information);
        msgBox.exec();
    });
}

QTextEdit* Editor::currentTextEdit()
{
    QWidget* current = tabWidget->currentWidget();
    if(current && tabsMap.find(current) != tabsMap.end())
    {
        return tabsMap[current].mainEditor;
    }
    return nullptr;
}

TabBundle Editor::currentBundle()
{
    QWidget* current = tabWidget->currentWidget();
    if(current && tabsMap.find(current) != tabsMap.end())
    {
        return tabsMap[current];
    }
    return TabBundle();
}

void Editor::addNewTab(const QString &title, const QString &filePath)
{
    QWidget *containerWidget = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(containerWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    QTextEdit *lineNumbers = new QTextEdit(containerWidget);
    lineNumbers->setFixedWidth(45);
    lineNumbers->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lineNumbers->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lineNumbers->setReadOnly(true);
    lineNumbers->setAlignment(Qt::AlignRight);
    lineNumbers->setFrameStyle(QFrame::NoFrame);

    QTextEdit *mainEditor = new QTextEdit(containerWidget);
    mainEditor->setFrameStyle(QFrame::NoFrame);
    mainEditor->setAcceptRichText(false);
    mainEditor->setLineWrapMode(QTextEdit::WidgetWidth);
    mainEditor->installEventFilter(this);
    mainEditor->document()->setUndoRedoEnabled(true);

    // Setup custom context menu tracking rules for spelling corrections on target edit surfaces
    mainEditor->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(mainEditor, &QTextEdit::customContextMenuRequested, this, [this, mainEditor](const QPoint &pos)
    {
        showCustomContextMenu(mainEditor, pos);
    });

    QObject::connect(mainEditor, &QTextEdit::cursorPositionChanged, this, [this, mainEditor]()
    {
        QTextCursor cursor = mainEditor->textCursor();
        int line = cursor.blockNumber() + 1;
        int col = cursor.columnNumber() + 1;

        if(cursorPositionLabel)
        {
            cursorPositionLabel->setText(QString("Line %1, Col %2").arg(line).arg(col));
        }

        applySyntaxHighlighting(mainEditor); 
        checkBracketMatching(mainEditor);
    });

    QTextEdit *minimap = new QTextEdit(containerWidget);
    minimap->setFixedWidth(110);
    minimap->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    minimap->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    minimap->setReadOnly(true);
    minimap->setFrameStyle(QFrame::NoFrame);
    minimap->setTextInteractionFlags(Qt::NoTextInteraction);

    minimap->document()->setUndoRedoEnabled(false);
    minimap->setAcceptDrops(false);
    minimap->document()->setDocumentLayout(nullptr);

    // Add an Event Filter to paint the tracking frame and process
    // click coordinates to scroll the main canvas
    minimap->viewport()->installEventFilter(this); 
    minimap->viewport()->setCursor(Qt::PointingHandCursor);
    
    // Track pointer states dynamically inside the widget properties dictionary
    minimap->setProperty("isMinimapViewport", true);
    minimap->setProperty("associatedMainEditor", QVariant::fromValue(mainEditor));

    QFont monoFont("Courier New", 11);
    mainEditor->setFont(monoFont);
    lineNumbers->setFont(monoFont);
    minimap->setFont(QFont("Courier New", 3));

    layout->addWidget(lineNumbers);
    layout->addWidget(mainEditor);
    layout->addWidget(minimap);

    int index = tabWidget->addTab(containerWidget, title);

    TabBundle bundle;
    bundle.mainEditor = mainEditor;
    bundle.lineNumberPanel = lineNumbers;
    bundle.minimapPanel = minimap;
    bundle.filePath = filePath;
    bundle.activeLangKey = detectLanguageKey(filePath);
    tabsMap[containerWidget] = bundle;

    tabWidget->setCurrentIndex(index);
    tabWidget->setTabToolTip(index, filePath.isEmpty() ? "New Document" : filePath);

    // Explicitly populate the line number column and minimap text layer *before* attaching loops.
    // This guarantees that a new blank tab immediately shows "1" in the gutter column.
    {
        QSignalBlocker lineBlocker(lineNumbers->document());
        lineNumbers->setPlainText("1\n");
    }
    {
        QSignalBlocker miniBlocker(minimap->document());
        minimap->setPlainText(mainEditor->toPlainText());
    }

    QObject::connect(mainEditor->verticalScrollBar(), &QScrollBar::valueChanged, this, [lineNumbers, mainEditor, minimap](int value)
    {
        if(lineNumbers && lineNumbers->verticalScrollBar())
        {
            lineNumbers->verticalScrollBar()->setValue(value);
        }

        QTimer::singleShot(0, mainEditor, [mainEditor, minimap, value]()
        {
            if(!mainEditor || !minimap ||
               !mainEditor->verticalScrollBar() || !minimap->verticalScrollBar())
                return;

            int mainMax = mainEditor->verticalScrollBar()->maximum();
            int miniMax = minimap->verticalScrollBar()->maximum();

            if(mainMax > 0)
            {
                double ratio = static_cast<double>(miniMax) / mainMax;
                minimap->verticalScrollBar()->setValue(static_cast<int>(value * ratio));
            }
            else
            {
                minimap->verticalScrollBar()->setValue(0);
            }

            minimap->viewport()->update();
        });
    });

    QTimer* throttleTimer = new QTimer(containerWidget);
    throttleTimer->setSingleShot(true);
    throttleTimer->setInterval(300); // Wait 300ms after user stops typing to sync heavy panels

    QObject::connect(mainEditor->document(), &QTextDocument::contentsChanged, this, [this, containerWidget, mainEditor, lineNumbers, minimap, throttleTimer]()
    {
        int tabIndex = tabWidget->indexOf(containerWidget);

        if(tabIndex != -1)
        {
            QString currentTitle = tabWidget->tabText(tabIndex);
            bool hasStar = currentTitle.endsWith("*");
            bool isModified = mainEditor->document()->isModified();

            if(isModified && !hasStar)
            {
                tabWidget->setTabText(tabIndex, currentTitle + "*");
            }
            else if(!isModified && hasStar)
            {
                currentTitle.chop(1);
                tabWidget->setTabText(tabIndex, currentTitle);
            }
        }

        // Start or reset the batch throttle timer. 
        // This stops mainEditor->toPlainText() from destroying the undo stack 
        // layout transaction on every keystroke.
        throttleTimer->start();
    });

    // Execute heavy UI adjustments only when the user pauses typing
    QObject::connect(throttleTimer, &QTimer::timeout, this, [this, containerWidget, mainEditor, lineNumbers, minimap]()
    {
        if(tabsMap.find(containerWidget) == tabsMap.end() ||
           !mainEditor || !lineNumbers || !minimap) return;

        updateStatusBar();
        updateLineNumbers(mainEditor, lineNumbers);

        // Sync minimap document text in a batch
        {
            QSignalBlocker miniBlocker(minimap->document());
            minimap->setPlainText(mainEditor->toPlainText());

            // If dark mode is active, explicitly force white text attributes
            if(isDarkMode)
            {
                QTextCursor cursor(minimap->document());
                cursor.select(QTextCursor::Document);
                QTextCharFormat whiteTextFormat;
                whiteTextFormat.setForeground(QColor(200, 200, 200));
                cursor.mergeCharFormat(whiteTextFormat);
            }
        }

        if(!mainEditor->toPlainText().isEmpty())
        {
            applySyntaxHighlighting(mainEditor);
            triggerAsyncSpellCheck(mainEditor);
        }
    });

    if(isDarkMode)
    {
        applyThemeToBundle(bundle);
    }
    else
    {
        lineNumbers->setStyleSheet("background-color: #f0f0f0; color: #888888;");
        minimap->setStyleSheet("background-color: #fafafa; opacity: 0.7;");
    }

    updateStatusBar();
}

void Editor::showCustomContextMenu(QTextEdit* editor, const QPoint &pos)
{
    if(!editor || !hunspellEngine) return;

    QMenu *menu = editor->createStandardContextMenu(pos);
    QTextCursor cursor = editor->cursorForPosition(pos);

    // Select word directly under right-clicked cursor location
    cursor.select(QTextCursor::WordUnderCursor);
    QString selectedWord = cursor.selectedText();

    if(!selectedWord.isEmpty())
    {
        std::string wordStr = selectedWord.toStdString();

        // Check word spelling using Hunspell engine API call bounds
        if(Hunspell_spell(hunspellEngine, wordStr.c_str()) == 0)
        {
            // Populate interactive replacement suggestion listings
            char** slist;
            int count = Hunspell_suggest(hunspellEngine, &slist, wordStr.c_str());

            if(count > 0)
            {
                menu->addSeparator();

                QMenu *suggestMenu = menu->addMenu(QString("Spelling Suggestions for '%1'")
                                                        .arg(selectedWord));

                for(int i = 0; i < count; ++i)
                {
                    QString suggestion = QString::fromUtf8(slist[i]);
                    QAction *action = suggestMenu->addAction(suggestion);

                    // token replacement
                    QObject::connect(action, &QAction::triggered, this, [editor, cursor, suggestion]() {
                        QTextCursor writeCursor = cursor;
                        writeCursor.insertText(suggestion);
                        editor->setTextCursor(writeCursor);
                    });
                }

                Hunspell_free_list(hunspellEngine, &slist, count);
            }
            else
            {
                menu->addSeparator();

                QAction *noSuggestAction = menu->addAction("No spelling suggestions found");
                noSuggestAction->setEnabled(false);
            }
        }
    }

    menu->exec(editor->mapToGlobal(pos));
    delete menu;
}

void Editor::updateLineNumbers(QTextEdit* mainEditor, QTextEdit* lineNumbers)
{
    if(!mainEditor || !lineNumbers) return;

    int currentBlocks = mainEditor->document()->blockCount();
    int existingLinesCount = lineNumbers->document()->blockCount();

    if(currentBlocks != existingLinesCount)
    {
        QSignalBlocker blocker(lineNumbers->document());
        QString linesText;

        for(int i = 1; i <= currentBlocks; ++i)
        {
            linesText += QString::number(i) + "\n";
        }

        lineNumbers->setPlainText(linesText);
    }
}

void Editor::applySyntaxHighlighting(QTextEdit *textEdit)
{
    if(!textEdit || !textEdit->document()) return;

    // Identify which language key this specific textEdit instance is currently tracking
    QString langKey = "txt";
    for(auto const& [container, bundle] : tabsMap)
    {
        if(bundle.mainEditor == textEdit)
        {
            langKey = bundle.activeLangKey;
            break;
        }
    }

    // Use a local list to hold formatting selections for the current view pass
    QList<QTextEdit::ExtraSelection> selections = textEdit->extraSelections();

    // Clear out previous syntax entries while preserving other persistent blocks if needed
    // For safety, let's filter or rebuild to isolate only syntax selections
    auto it = std::remove_if(selections.begin(), selections.end(), [](const QTextEdit::ExtraSelection& sel) 
    {
        return sel.format.property(QTextFormat::UserProperty).toInt() == 101; // Marker for syntax rules
    });
    selections.erase(it, selections.end());

    // IF file is a plain text file (.txt) or an unsaved "Untitled" tab,
    // skip keyword processing completely.
    if(langKey == "txt")
    {
        textEdit->setExtraSelections(selections);
        return;
    }

    QTextDocument *doc = textEdit->document();
    QString text = doc->toPlainText();

    // Structural range mask vector storing [StartPos, EndPos] of comment blocks
    std::vector<std::pair<int, int>> commentMask;
    const auto& activeRules = languageRulesRepository[langKey];

    // PASS 1A: Process MULTILINE comments first to establish master exclusion boundaries
    for(const auto &rule : activeRules)
    {
        if(rule.format.fontItalic())
        {
            // Identify multiline comment patterns by checking if the regex text matches the "/*" identifier block
            if(rule.pattern.pattern().contains("/\\*"))
            {
                QRegularExpressionMatchIterator i = rule.pattern.globalMatch(text);
                while(i.hasNext())
                {
                    QRegularExpressionMatch match = i.next();
                    int start = match.capturedStart();
                    int end = match.capturedEnd();
                    
                    commentMask.push_back({start, end});

                    QTextEdit::ExtraSelection selection;
                    selection.format = rule.format;
                    selection.format.setProperty(QTextFormat::UserProperty, 101);
                    selection.cursor = QTextCursor(doc);
                    selection.cursor.setPosition(start);
                    selection.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, match.capturedLength());
                    selections.append(selection);
                }
            }
        }
    }

    // PASS 1B: Process SINGLE-LINE comments, skipping any '//' that fall inside an active block comment
    for(const auto &rule : activeRules)
    {
        if(rule.format.fontItalic())
        {
            // Target single-line patterns (skipping the multiline block rules we already computed)
            if(!rule.pattern.pattern().contains("/\\*"))
            {
                QRegularExpressionMatchIterator i = rule.pattern.globalMatch(text);
                while(i.hasNext())
                {
                    QRegularExpressionMatch match = i.next();
                    int start = match.capturedStart();
                    int end = match.capturedEnd();

                    // Guard: Verify if this single-line comment token is sitting inside a /* ... */ block
                    bool insideBlockComment = false;
                    for(const auto& bounds : commentMask)
                    {
                        if(start >= bounds.first && start < bounds.second)
                        {
                            insideBlockComment = true;
                            break;
                        }
                    }

                    // Only apply format if the single-line comment is truly standalone
                    if(!insideBlockComment)
                    {
                        commentMask.push_back({start, end});

                        QTextEdit::ExtraSelection selection;
                        selection.format = rule.format;
                        selection.format.setProperty(QTextFormat::UserProperty, 101);
                        selection.cursor = QTextCursor(doc);
                        selection.cursor.setPosition(start);
                        selection.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, match.capturedLength());
                        selections.append(selection);
                    }
                }
            }
        }
    }

    // PASS 2: Process string literals, numeric values, preprocessor commands, and keywords
    for(const auto &rule : activeRules)
    {
        // Skip comment patterns since they were fully handled during the first pass
        if(rule.format.fontItalic()) continue;

        QRegularExpressionMatchIterator i = rule.pattern.globalMatch(text);
        while(i.hasNext())
        {
            QRegularExpressionMatch match = i.next();
            int matchStart = match.capturedStart();

            // Verify if the current match overlaps with an established comment boundary range
            bool insideComment = false;
            for(const auto& bounds : commentMask)
            {
                if(matchStart >= bounds.first && matchStart < bounds.second)
                {
                    insideComment = true;
                    break;
                }
            }

            // Only append the code keyword format if the text position is outside comment masks
            if(!insideComment)
            {
                QTextEdit::ExtraSelection selection;
                selection.format = rule.format;
                selection.format.setProperty(QTextFormat::UserProperty, 101);
                selection.cursor = QTextCursor(doc);
                selection.cursor.setPosition(matchStart);
                selection.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, match.capturedLength());
                selections.append(selection);
            }
        }
    }

    textEdit->setExtraSelections(selections);
}

void Editor::closeTab(int index)
{
    QWidget *containerWidget = tabWidget->widget(index);
    if(!containerWidget) return;

    if(tabsMap.find(containerWidget) != tabsMap.end())
    {
        QTextEdit* editor = tabsMap[containerWidget].mainEditor;

        // If the tab has modified changes, warn the user first
        if(editor && editor->document()->isModified())
        {
            QString fileName = tabWidget->tabText(index).remove('*');
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Unsaved Changes",
                QString("The document '%1' has unsaved changes. Are you sure you want to close it?")
                                .arg(fileName),
                QMessageBox::Yes | QMessageBox::No
            );

            if(reply == QMessageBox::No)
            {
                return;
            }
        }

        if(editor)
        {
            // Lock the background thread engine state
            std::lock_guard<std::mutex> lock(textMutex);

            if(spellCheckTargetEdit == editor)
            {
                spellCheckTargetEdit = nullptr;
            }

            if(editor->document())
            {
                editor->document()->blockSignals(true);
            }

            editor->removeEventFilter(this);
        }

        if(tabsMap[containerWidget].minimapPanel)
        {
            tabsMap[containerWidget].minimapPanel->viewport()->removeEventFilter(this);
        }

        tabsMap.erase(containerWidget);
    }

    tabWidget->removeTab(index);
    containerWidget->deleteLater();

    if(tabWidget->count() == 0) addNewTab();
}

void Editor::openFile(const QString &fileName)
{
    if(fileName.isEmpty()) return;

    // Check if file is already open in an existing tab
    for(auto const& [container, bundle] : tabsMap)
    {
        if(bundle.filePath == fileName)
        {
            int tabIndex = tabWidget->indexOf(container);

            if(tabIndex != -1)
            {
                tabWidget->setCurrentIndex(tabIndex);
            }

            return;
        }
    }

    QFile file(fileName);

    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Error", "Cannot open file: " + file.errorString());
        return;
    }

    QString baseName = QFileInfo(fileName).fileName();
    addNewTab(baseName, fileName);

    QWidget* container = tabWidget->currentWidget();

    if(container && tabsMap.find(container) != tabsMap.end())
    {
        TabBundle &bundle = tabsMap[container];
        bundle.activeLangKey = detectLanguageKey(fileName);
        
        if(bundle.mainEditor)
        {
            // Block signals so loading doesn't taint our pristine stack or instantly add stars
            QSignalBlocker textBlocker(bundle.mainEditor->document());

            QTextStream in(&file);
            bundle.mainEditor->setText(in.readAll());
            bundle.mainEditor->document()->setModified(false);

            // Manually force synchronous population passes here 
            // since the main signal pipelines are blocked during load.
            updateLineNumbers(bundle.mainEditor, bundle.lineNumberPanel);

            if(bundle.minimapPanel)
            {
                QSignalBlocker miniBlocker(bundle.minimapPanel->document());
                bundle.minimapPanel->setPlainText(bundle.mainEditor->toPlainText());
            }

            applySyntaxHighlighting(bundle.mainEditor);
        }
    }

    adjustRecentFiles(fileName);
    updateStatusBar();
}

void Editor::saveFile()
{
    QWidget* container = tabWidget->currentWidget();
    if(!container || tabsMap.find(container) == tabsMap.end()) return;

    TabBundle &bundle = tabsMap[container];

    // If there is no file path assigned yet, it's a new document. Force "Save As".
    if(bundle.filePath.isEmpty())
    {
        saveCurrentFile();
        return;
    }

    QFile file(bundle.filePath);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Error", "Cannot save file: " + file.errorString());
        return;
    }

    QTextStream out(&file);
    out << bundle.mainEditor->toPlainText();
    bundle.mainEditor->document()->setModified(false);

    // Clear out the modification indicator star
    int index = tabWidget->currentIndex();
    QString baseName = QFileInfo(bundle.filePath).fileName();
    tabWidget->setTabText(index, baseName);

    updateStatusBar();
}

void Editor::saveCurrentFile()
{
    TabBundle bundle = currentBundle();
    if(!bundle.mainEditor) return;

    QString fileName = QFileDialog::getSaveFileName(this, "Save file", bundle.filePath);
    if(fileName.isEmpty()) return;

    QFile file(fileName);

    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Error", "Cannot save file: " + file.errorString());
        return;
    }

    QWidget* container = tabWidget->currentWidget();

    if(container)
    {
        tabsMap[container].filePath = fileName;
        tabsMap[container].activeLangKey = detectLanguageKey(fileName);

        int index = tabWidget->currentIndex();
        tabWidget->setTabText(index, QFileInfo(fileName).fileName());

        applySyntaxHighlighting(tabsMap[container].mainEditor);
    }

    QTextStream out(&file);
    out << bundle.mainEditor->toPlainText();
    bundle.mainEditor->document()->setModified(false);

    // Clear out the modification indicator star
    int currentIndex = tabWidget->currentIndex();
    QString baseName = QFileInfo(fileName).fileName();
    tabWidget->setTabText(currentIndex, baseName);

    adjustRecentFiles(fileName);
    updateStatusBar();
}

void Editor::closeEvent(QCloseEvent *event)
{
    bool hasUnsavedChanges = false;

    // Scan through all open workspace tabs checking modification
    for(auto const& [container, bundle] : tabsMap)
    {
        if(bundle.mainEditor && bundle.mainEditor->document()->isModified())
        {
            hasUnsavedChanges = true;
            break;
        }
    }

    if(hasUnsavedChanges)
    {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Unsaved Changes",
                                      "You have documents with unsaved changes. Do you really want to exit?",
                                      QMessageBox::Yes | QMessageBox::No);
        
        if(reply == QMessageBox::No)
        {
            event->ignore();
            return;
        }
    }

    event->accept();
}

void Editor::exportCurrentToPdf()
{
    TabBundle bundle = currentBundle();
    if(!bundle.mainEditor) return;

    QString savePath = QFileDialog::getSaveFileName(this, "Export Document to PDF", "", "PDF Files (*.pdf)");
    if(savePath.isEmpty()) return;

    QFileInfo fileInfo(savePath);
    if(fileInfo.suffix().isEmpty()) savePath += ".pdf";

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(savePath);
    printer.setPageSize(QPageSize(QPageSize::A4));
    bundle.mainEditor->print(&printer);

    QMessageBox::information(this, "Export Success", "Document successfully exported to PDF.");
}

void Editor::setupAutoSave()
{
    autoSaveTimer = new QTimer(this);
    autoSaveTimer->setInterval(30000);

    QObject::connect(autoSaveTimer, &QTimer::timeout, this, [this]()
    {
        for(auto const& [container, bundle] : tabsMap)
        {
            if(bundle.mainEditor && !bundle.filePath.isEmpty() && 
               bundle.mainEditor->document()->isModified())
            {
                QFile file(bundle.filePath);

                if(file.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    QTextStream out(&file);
                    out << bundle.mainEditor->toPlainText();
                    bundle.mainEditor->document()->setModified(false);

                    // Strip the trailing modification star from this tab text field
                    int tabIdx = tabWidget->indexOf(container);
                    if(tabIdx != -1)
                    {
                        QString currentText = tabWidget->tabText(tabIdx);
                        if(currentText.endsWith("*"))
                        {
                            currentText.chop(1);
                            tabWidget->setTabText(tabIdx, currentText);
                        }
                    }
                }
            }
        }

        updateStatusBar();
    });

    autoSaveTimer->start();
}

void Editor::adjustRecentFiles(const QString &filePath)
{
    recentFilesList.removeAll(filePath);
    recentFilesList.prepend(filePath);

    while(recentFilesList.size() > MaxRecentFiles) recentFilesList.removeLast();

    QSettings settings("LaylaOS", "TextEditor");
    settings.setValue("recentFiles", recentFilesList);
    updateRecentFilesMenu();
}

void Editor::loadRecentFiles()
{
    QSettings settings("LaylaOS", "TextEditor");
    recentFilesList = settings.value("recentFiles").toStringList();
}

void Editor::updateRecentFilesMenu()
{
    recentFilesMenu->clear();

    if(recentFilesList.isEmpty())
    {
        QAction *noRecent = recentFilesMenu->addAction("No recent files");
        noRecent->setEnabled(false);
        return;
    }

    for(const QString &file : recentFilesList)
    {
        QAction *action = recentFilesMenu->addAction(QFileInfo(file).fileName());
        QObject::connect(action, &QAction::triggered, this, [this, file]() { openFile(file); });
    }
}

void Editor::triggerAsyncSpellCheck(QTextEdit* activeEdit)
{
    std::lock_guard<std::mutex> lock(textMutex);
    spellCheckTargetEdit = activeEdit;
    textToValidate = activeEdit->toPlainText().toStdString();
    spellCheckPending = true;
}

void Editor::startSpellCheckWorker()
{
    spellCheckThread = std::thread([this]()
    {
        while(!stopSpellCheck)
        {
            if(spellCheckPending && hunspellEngine)
            {
                spellCheckPending = false;
                std::string currentText;
                {
                    std::lock_guard<std::mutex> lock(textMutex);
                    currentText = textToValidate;
                }

                std::vector<std::pair<int, int>> misspelledWordRanges;
                std::string currentWord = "";
                int wordStartPos = -1;
                int textLen = static_cast<int>(currentText.length());

                for(size_t i = 0; i < textLen; ++i)
                {
                    char c = currentText[i];

                    if(std::isalpha(static_cast<unsigned char>(c)))
                    {
                        if(wordStartPos == -1) wordStartPos = static_cast<int>(i);
                        currentWord += c;
                    }
                    else
                    {
                        if(!currentWord.empty())
                        {
                            if(Hunspell_spell(hunspellEngine, currentWord.c_str()) == 0)
                            {
                                if(wordStartPos + static_cast<int>(currentWord.length()) <= textLen)
                                {
                                    misspelledWordRanges.push_back({
                                        wordStartPos, static_cast<int>(currentWord.length())
                                    });
                                }
                            }

                            currentWord = "";
                            wordStartPos = -1;
                        }
                    }
                }

                if(!currentWord.empty() && Hunspell_spell(hunspellEngine, currentWord.c_str()) == 0)
                {
                    if(wordStartPos + static_cast<int>(currentWord.length()) <= textLen)
                    {
                        misspelledWordRanges.push_back({
                            wordStartPos, static_cast<int>(currentWord.length())
                        });
                    }
                }

                QMetaObject::invokeMethod(this, [this, misspelledWordRanges]()
                {
                    std::lock_guard<std::mutex> lock(textMutex);
                    highlightMisspellings(spellCheckTargetEdit, misspelledWordRanges);
                }, Qt::QueuedConnection);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    });
}

void Editor::highlightMisspellings(QTextEdit* targetEdit, const std::vector<std::pair<int, int>>& ranges)
{
    if(!targetEdit) return;

    bool targetStillExists = false;

    for(auto const& [container, bundle] : tabsMap)
    {
        if(bundle.mainEditor == targetEdit)
        {
            targetStillExists = true;
            break;
        }
    }

    if(!targetStillExists) return;

    // Filter and preserve only non-spellcheck extra selections
    QList<QTextEdit::ExtraSelection> selections = targetEdit->extraSelections();
    auto it = std::remove_if(selections.begin(), selections.end(), [](const QTextEdit::ExtraSelection& sel) 
    {
        return sel.format.property(QTextFormat::UserProperty).toInt() == 103; // Marker for spellcheck
    });
    selections.erase(it, selections.end());

    QTextCharFormat errorFormat;
    errorFormat.setUnderlineColor(Qt::red);
    errorFormat.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
    errorFormat.setProperty(QTextFormat::UserProperty, 103); // Tag as spellcheck rule

    int maxSafeIndex = targetEdit->document()->characterCount() - 1;

    for(const auto& range : ranges)
    {
        if(range.first + range.second <= maxSafeIndex && range.first >= 0)
        {
            QTextEdit::ExtraSelection sel;
            sel.format = errorFormat;
            sel.cursor = QTextCursor(targetEdit->document());
            sel.cursor.setPosition(range.first);
            sel.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, range.second);
            selections.append(sel);
        }
    }

    // Safely inject red underlines without polluting document metrics
    targetEdit->setExtraSelections(selections);
}

void Editor::updateStatusBar()
{
    TabBundle bundle = currentBundle();

    if(!bundle.mainEditor)
    {
        statusLabel->setText("No Active Documents Open");
        return;
    }

    QString text = bundle.mainEditor->toPlainText();
    int charCount = text.length();
    int wordCount = text.isEmpty() ? 0 : text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).count();

    QString autoSaveStatus = bundle.filePath.isEmpty() ? "Disabled (Save first)" : "Active (30s)";

    statusLabel->setText(QString("Words: %1 | Characters: %2 | Auto-save: %3 | Total Tabs: %4")
        .arg(wordCount)
        .arg(charCount)
        .arg(autoSaveStatus)
        .arg(tabWidget->count()));
}

void Editor::showFindReplaceDialog()
{
    TabBundle bundle = currentBundle();
    if(!bundle.mainEditor) return;

    QDialog dialog(this);
    dialog.setWindowTitle("Find and Replace");

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    QLineEdit *findEdit = new QLineEdit(&dialog);
    findEdit->setPlaceholderText("Find text...");
    mainLayout->addWidget(findEdit);

    QLineEdit *replaceEdit = new QLineEdit(&dialog);
    replaceEdit->setPlaceholderText("Replace with...");
    mainLayout->addWidget(replaceEdit);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *btnFindNext = new QPushButton("Find Next", &dialog);
    QPushButton *btnReplace = new QPushButton("Replace All", &dialog);
    btnLayout->addWidget(btnFindNext);
    btnLayout->addWidget(btnReplace);
    mainLayout->addLayout(btnLayout);

    QObject::connect(btnFindNext, &QPushButton::clicked, bundle.mainEditor, [bundle, findEdit]()
    {
        QString searchString = findEdit->text();
        if(searchString.isEmpty()) return;

        if(!bundle.mainEditor->find(searchString))
        {
            bundle.mainEditor->moveCursor(QTextCursor::Start);
            bundle.mainEditor->find(searchString);
        }
    });

    QObject::connect(btnReplace, &QPushButton::clicked, bundle.mainEditor, [bundle, findEdit, replaceEdit]() 
    {
        QString searchString = findEdit->text();
        QString replaceString = replaceEdit->text();
        if(searchString.isEmpty()) return;

        QString currentText = bundle.mainEditor->toPlainText();
        currentText.replace(searchString, replaceString);
        bundle.mainEditor->setText(currentText);
    });

    if(isDarkMode) dialog.setPalette(this->palette());
    dialog.exec();
}

void Editor::applyThemeToBundle(const TabBundle& bundle)
{
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(40, 40, 40));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::Text, QColor(220, 220, 220));
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);

    if(bundle.mainEditor)
    {
        bundle.mainEditor->setPalette(darkPalette);
    }

    if(bundle.lineNumberPanel)
    {
        bundle.lineNumberPanel->setPalette(darkPalette);
        bundle.lineNumberPanel->setStyleSheet("background-color: #222222; color: #888888;");
    }

    if(bundle.minimapPanel)
    {
        bundle.minimapPanel->setPalette(darkPalette);

        // Using explicit css rules overrides internal document font caches
        bundle.minimapPanel->setStyleSheet(
            "background-color: #282828;"
            "color: #ffffff;"
        );

        // Force the layout engine to refresh the current display layer text canvas rules
        QSignalBlocker blocker(bundle.minimapPanel->document());
        QTextCursor cursor(bundle.minimapPanel->document());
        cursor.select(QTextCursor::Document);
        QTextCharFormat whiteTextFormat;
        whiteTextFormat.setForeground(QColor(200, 200, 200));
        cursor.mergeCharFormat(whiteTextFormat);
    }
}

void Editor::toggleDarkMode()
{
    isDarkMode = !isDarkMode;
    QPalette paletteTarget;

    if(isDarkMode)
    {
        paletteTarget.setColor(QPalette::Window, QColor(53, 53, 53));
        paletteTarget.setColor(QPalette::WindowText, Qt::white);
        paletteTarget.setColor(QPalette::Base, QColor(40, 40, 40));
        paletteTarget.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        paletteTarget.setColor(QPalette::ToolTipBase, Qt::white);
        paletteTarget.setColor(QPalette::ToolTipText, Qt::white);
        paletteTarget.setColor(QPalette::Text, QColor(220, 220, 220));
        paletteTarget.setColor(QPalette::Button, QColor(53, 53, 53));
        paletteTarget.setColor(QPalette::ButtonText, Qt::white);
        paletteTarget.setColor(QPalette::BrightText, Qt::red);
        paletteTarget.setColor(QPalette::Link, QColor(42, 130, 218));
        //paletteTarget.setColor(QPalette::Highlight, QColor(42, 130, 218));
        //paletteTarget.setColor(QPalette::HighlightedText, Qt::white);

        if(fileTreeView)
        {
            fileTreeView->setPalette(paletteTarget);
            fileTreeView->setStyleSheet("background-color: #353535; color: white;");
        }
    }
    else
    {
        paletteTarget = style()->standardPalette();

        if(fileTreeView)
        {
            fileTreeView->setPalette(paletteTarget);
            fileTreeView->setStyleSheet("background-color: #ffffff; color: #000000;");
        }
    }

    setPalette(paletteTarget);

    for(auto const& [container, bundle] : tabsMap)
    {
        if(isDarkMode)
        {
            applyThemeToBundle(bundle);
        }
        else
        {
            if(bundle.mainEditor) bundle.mainEditor->setPalette(paletteTarget);

            if(bundle.lineNumberPanel)
            {
                bundle.lineNumberPanel->setPalette(paletteTarget);
                bundle.lineNumberPanel->setStyleSheet("background-color: #f0f0f0; color: #888888;");
            }

            if(bundle.minimapPanel)
            {
                bundle.minimapPanel->setPalette(paletteTarget);
                bundle.minimapPanel->setStyleSheet("background-color: #fafafa;");
            }
        }
    }
}

void Editor::checkBracketMatching(QTextEdit* editor)
{
    if(!editor || !editor->document()) return;

    // Preserve existing syntax highlights, but clear prior bracket selections
    QList<QTextEdit::ExtraSelection> selections = editor->extraSelections();
    auto it = std::remove_if(selections.begin(), selections.end(), [](const QTextEdit::ExtraSelection& sel) 
    {
        return sel.format.property(QTextFormat::UserProperty).toInt() == 102; // Marker for brackets
    });
    selections.erase(it, selections.end());

    QTextDocument* doc = editor->document();
    QTextCursor cursor = editor->textCursor();
    int currentPos = cursor.position();
    QString fullText = doc->toPlainText();

    QTextCharFormat matchFormat;
    matchFormat.setUnderlineColor(Qt::cyan);
    matchFormat.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    matchFormat.setFontWeight(QFont::Bold);
    matchFormat.setProperty(QTextFormat::UserProperty, 102); // Tag as bracket highlight
    matchFormat.setBackground(QColor(0, 255, 0, 80)); // Soft Green Highlight background

    char prevCh = (currentPos > 0) ? fullText[currentPos - 1].toLatin1() : '\0';

    auto addBracketSelection = [doc, matchFormat, &selections](int pos)
    {
        QTextEdit::ExtraSelection sel;
        sel.format = matchFormat;
        sel.cursor = QTextCursor(doc);
        sel.cursor.setPosition(pos);
        sel.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
        selections.append(sel);
    };

    // Forward Search for Matching Closing Brace '}'
    if(prevCh == '{' || prevCh == '(')
    {
        char closeCh = (prevCh == '{') ? '}' : ')';
        int depth = 1;
        for(int i = currentPos; i < fullText.length(); ++i)
        {
            if(fullText[i] == prevCh) depth++;
            else if(fullText[i] == closeCh) depth--;
            if(depth == 0)
            {
                addBracketSelection(currentPos - 1);
                addBracketSelection(i);
                break;
            }
        }
    }
    // Backward Search for Matching Opening Brace '{'
    else if (prevCh == '}' || prevCh == ')')
    {
        char openCh = (prevCh == '}') ? '{' : '(';
        int depth = 1;
        for(int i = currentPos - 2; i >= 0; --i)
        {
            if(fullText[i] == prevCh) depth++;
            else if(fullText[i] == openCh) depth--;
            if(depth == 0)
            {
                addBracketSelection(i);
                addBracketSelection(currentPos - 1);
                break;
            }
        }
    }

    // Render the bracket underlines onto the display viewport layout layer
    editor->setExtraSelections(selections);
}

void Editor::toggleSidebar()
{
    if(!mainSplitter) return;

    QList<int> sizes = mainSplitter->sizes();
    if(sizes.isEmpty()) return;

    if(sizes[0] > 0)
    {
        // Sidebar is currently expanded. Record width and collapse it.
        lastSidebarWidth = sizes[0];
        mainSplitter->setSizes(QList<int>({0, sizes[0] + sizes[1]}));
    }
    else
    {
        // Sidebar is collapsed. Expand it back using the recorded width.
        mainSplitter->setSizes(QList<int>({lastSidebarWidth, this->width() - lastSidebarWidth}));
    }
}

