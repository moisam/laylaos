
STATIC_INLINE int ipv4_is_broadcast(uint32_t addr, uint32_t netmask)
{
    addr = htonl(addr);
    netmask = htonl(netmask);

    if((addr & ~netmask) == (0xFFFFFFFF & ~netmask))
    {
        return 1;
    }

    return (addr == 0x00 || addr == 0xffffffff);
}


STATIC_INLINE int ipv4_is_multicast(uint32_t addr)
{
    addr = htonl(addr);
    return ((addr & 0xF0000000) == 0xE0000000);
}


STATIC_INLINE int ipv4_is_same_network(uint32_t addr1, uint32_t addr2, uint32_t netmask)
{
    addr1 = htonl(addr1);
    addr2 = htonl(addr2);
    netmask = htonl(netmask);
    return (addr1 & netmask) == (addr2 & netmask);
}


/*
 * Return the byte at the given index from the given IP address.
 */
STATIC_INLINE int ipaddr_byte(uint32_t addr, int byte)
{
    static int shift[] = { 0, 8, 16, 24 };

    addr = htonl(addr);
    return (addr >> shift[byte]) & 0xFF;
}

