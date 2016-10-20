/*
 * ping.cpp
 *
 * Created on: 2016-02-08
 *     Author: patw
 */

#include <syslog.h>

#include "ping.h"
#include "utils.h"

// Define some constants.
#define IP4_HDRLEN 20         // IPv4 header length
#define ICMP_HDRLEN 8         // ICMP header length for echo request, excludes data
#define BUFSIZE 8192


uint16_t Ping::checksum (void* addr, int len)
{
    uint32_t sum = 0;
    uint16_t* pBuf = (uint16_t*)addr;

    // Main summing loop
    while (len > 1) {
        sum +=  *pBuf++;
        len -= sizeof(uint16_t);
    }

    // Add left-over byte, if any
    if (len > 0) {
        sum +=  *(uint8_t*)pBuf;
    }

    // Fold 32-bit sum to 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)~sum;
}

uint32_t Ping::getRandom32()
{
    static uint32_t seed = 0;

    if (!seed) {
        struct timeval tv;
        gettimeofday(&tv, 0);
        seed = tv.tv_sec * 1000000 + tv.tv_usec;
        srand(seed);
    }

    return rand();
}

int Ping::readNlSock(int sockFd, uint8_t* bufPtr, uint32_t seqNum, uint32_t pId)
{
    struct nlmsghdr* nlHdr;
    uint32_t readLen = 0, msgLen = 0;

    do {
        /* Receive response from the kernel */
        if ((readLen = recv(sockFd, bufPtr, BUFSIZE - msgLen, 0)) < 0) {
            throw exceptionLevel("SOCK READ: ", true);
        }

        nlHdr = (struct nlmsghdr*)bufPtr;

        /* Check if the header is valid */
        if ((NLMSG_OK(nlHdr, readLen) == 0) || (nlHdr->nlmsg_type == NLMSG_ERROR)) {
            throw exceptionLevel("Error in received packet", true);
        }

        /* Check if the its the last message */
        if(nlHdr->nlmsg_type == NLMSG_DONE) {
            break;
        } else {
            /* Else move the pointer to buffer appropriately */
            bufPtr += readLen;
            msgLen += readLen;
        }

        /* Check if its a multi part message */
        if((nlHdr->nlmsg_flags & NLM_F_MULTI) == 0) {
            /* return if its not */
            break;
        }
    } while((nlHdr->nlmsg_seq != seqNum) || (nlHdr->nlmsg_pid != pId));

    return msgLen;
}

/* For parsing the route info returned */
void Ping::parseRouteInfo(struct nlmsghdr* nlHdr, RouteInfo_t* pRtInfo)
{
    struct rtmsg*  rtMsg;
    struct rtattr* rtAttr;
    int            rtLen;
    RouteInfo_t    tmpRtInfo;

    rtMsg = (struct rtmsg*)NLMSG_DATA(nlHdr);

    // the route is not for AF_INET or does not belong to main routing table
    if((rtMsg->rtm_family != AF_INET) || (rtMsg->rtm_table != RT_TABLE_MAIN)) {
        return;
    }

    memset(&tmpRtInfo, 0, sizeof(tmpRtInfo));

    /* get the rtattr field */
    rtAttr = (struct rtattr*)RTM_RTA(rtMsg);
    rtLen = RTM_PAYLOAD(nlHdr);

    for (; RTA_OK(rtAttr, rtLen); rtAttr = RTA_NEXT(rtAttr, rtLen)) {
        switch(rtAttr->rta_type) {
            case RTA_OIF:
                tmpRtInfo.ifIndex = *(uint32_t*)RTA_DATA(rtAttr);
                break;

            case RTA_GATEWAY:
                tmpRtInfo.gwAddr = *(u_int*)RTA_DATA(rtAttr);
                break;

            case RTA_PREFSRC:
                tmpRtInfo.srcAddr = *(u_int*)RTA_DATA(rtAttr);
                break;

            case RTA_DST:
                tmpRtInfo.destAddr = *(u_int*)RTA_DATA(rtAttr);
                break;
        }
    }

    if (tmpRtInfo.destAddr == 0) {
        // this is the default route
        pRtInfo->gwAddr = tmpRtInfo.gwAddr;
        pRtInfo->ifIndex = tmpRtInfo.ifIndex;
    }

    if ( tmpRtInfo.srcAddr && (tmpRtInfo.ifIndex == pRtInfo->ifIndex) ) {
        pRtInfo->srcAddr = tmpRtInfo.srcAddr;
    }
}

void Ping::printRouteInfo(RouteInfo_t* pRtInfo)
{
    struct in_addr in;
    char srcAddrStr[16];
    char gwAddrStr[16];
    char ifName[IF_NAMESIZE];

    in.s_addr = pRtInfo->srcAddr;
    strncpy(srcAddrStr, (char*)inet_ntoa(in), sizeof(srcAddrStr));

    in.s_addr = pRtInfo->gwAddr;
    strncpy(gwAddrStr, (char*)inet_ntoa(in), sizeof(gwAddrStr));

    if_indextoname(pRtInfo->ifIndex, ifName);

    std::cout << ifName << " " << srcAddrStr << " gw:" << gwAddrStr << endl;
}

void Ping::getRouteInfo(RouteInfo_t* pRtInfo)
{
    struct nlmsghdr* pNlMsg;
    uint8_t          msgBuf[BUFSIZE];

    int sock;
    int len;
    uint32_t msgSeq = 0;

    /* Create Socket */
    if ( (sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0 ) {
        throw exceptionLevel("Socket creation", true);
    }

    memset(msgBuf, 0, sizeof(msgBuf));

    // point the header and the msg structure pointers into the buffer
    pNlMsg = (struct nlmsghdr*)msgBuf;
    //struct rtmsg* pRtMsg = (struct rtmsg*)NLMSG_DATA(pNlMsg);

    // Fill in the nlmsg header
    pNlMsg->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    pNlMsg->nlmsg_type = RTM_GETROUTE; // Get the routes from kernel routing table.

    pNlMsg->nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST; // The message is a request for dump.
    pNlMsg->nlmsg_seq = msgSeq++;
    pNlMsg->nlmsg_pid = getpid();

    try {
        /* Send the request */
        send(sock, pNlMsg, pNlMsg->nlmsg_len, 0);
        /* Read the response */
        len = readNlSock(sock, msgBuf, msgSeq, getpid());
    } catch (exceptionLevel& e) {
        close(sock);
        throw e;
    } catch (std::exception& e) {
        close(sock);
        throw e;
    }

    while (NLMSG_OK(pNlMsg, len)) {
        parseRouteInfo(pNlMsg, pRtInfo);
        pNlMsg = NLMSG_NEXT(pNlMsg, len);
    }

    close(sock);
}

void Ping::ping(in_addr_t destAddr, in_addr_t srcAddr, uint32_t ifIndex, uint8_t* data, uint32_t datalen, uint16_t msgSeq)
{
    int sd;
    struct ip iphdr;
    struct icmp icmphdr;
    uint8_t* packet;
    struct sockaddr_in sin;
    struct ifreq ifr;
    int status = EXIT_SUCCESS;

    // Allocate memory for various arrays.

    packet = new uint8_t[IP_MAXPACKET];

    if (!packet) {
        throw exceptionLevel("Cannot allocate memory for array 'packet'.", true);
    }

    memset(packet, 0, IP_MAXPACKET);
    memset(&ifr, 0, sizeof (ifr));
    memset(&iphdr, 0, sizeof(iphdr));

    // IPv4 header
    //
    // IPv4 header length (4 bits): Number of 32-bit words in header = 5
    iphdr.ip_hl = IP4_HDRLEN / 4;

    // Internet Protocol version (4 bits): IPv4
    iphdr.ip_v = 4;

    // Type of service (8 bits)
    iphdr.ip_tos = 0;

    // Total length of datagram (16 bits): IP header + ICMP header + ICMP data
    iphdr.ip_len = htons (IP4_HDRLEN + ICMP_HDRLEN + datalen);

    // ID sequence number (16 bits): unused, since single datagram
    iphdr.ip_id = htons (0);

    // Flags, and Fragmentation offset (3, 13 bits): 0 since single datagram
    iphdr.ip_off = 0;

    // Time-to-Live (8 bits):
    iphdr.ip_ttl = 4;

    // Transport layer protocol (8 bits): 1 for ICMP
    iphdr.ip_p = IPPROTO_ICMP;

    // Source IPv4 address (32 bits)
    //inet_pton (AF_INET, src_ip, &(iphdr.ip_src));
    iphdr.ip_src.s_addr = srcAddr;

    // Destination IPv4 address (32 bits)
    //inet_pton (AF_INET, dst_ip, &iphdr.ip_dst);
    iphdr.ip_dst.s_addr = destAddr;
    ifr.ifr_ifindex = ifIndex;

    // IPv4 header checksum (16 bits): set to 0 when calculating checksum
    iphdr.ip_sum = 0;
    iphdr.ip_sum = checksum (&iphdr, IP4_HDRLEN);

    // ICMP header

    // Message Type (8 bits): echo request
    icmphdr.icmp_type = ICMP_ECHO;

    // Message Code (8 bits): echo request
    icmphdr.icmp_code = 0;

    // Identifier (16 bits): usually pid of sending process
    icmphdr.icmp_id = htons (getpid());

    // Sequence Number (16 bits): starts at 0
    icmphdr.icmp_seq = htons (msgSeq);

    // ICMP header checksum (16 bits): set to 0 when calculating checksum
    icmphdr.icmp_cksum = 0;

    // Prepare packet.
    //
    // Next part of packet is upper layer protocol header.
    memcpy(packet, &icmphdr, ICMP_HDRLEN);

    // Finally, add the ICMP data.
    memcpy(packet + ICMP_HDRLEN, data, datalen);

    // Calculate ICMP header checksum
    icmphdr.icmp_cksum = checksum (packet, ICMP_HDRLEN + datalen);
    memcpy(packet, &icmphdr, ICMP_HDRLEN);

    // The kernel is going to prepare layer 2 information (ethernet frame header) for us.
    // For that, we need to specify a destination for the kernel in order for it
    // to decide where to send the raw datagram. We fill in a struct in_addr with
    // the desired destination IP address, and pass this structure to the sendto() function.
    memset (&sin, 0, sizeof (struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = iphdr.ip_dst.s_addr;

    // Submit request for a raw socket descriptor.
    if ((sd = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
        delete[] packet;
        throw exceptionLevel("socket() failed ", true);
    }

    // Bind socket to interface index
    if (setsockopt (sd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof (ifr)) < 0) {
        delete[] packet;
        close (sd);
        throw exceptionLevel("setsockopt() failed to bind to interface ", true);
    }

    bind(sd, (struct sockaddr*)&sin, sizeof(sin));

    // Send packet.
    if (sendto(sd, packet, ICMP_HDRLEN + datalen, 0, (struct sockaddr*) &sin, sizeof (struct sockaddr)) < 0)  {
        delete[] packet;
        close (sd);
        throw exceptionLevel("sendto() failed ", true);
    }

    delete[] packet;
    packet = 0;

    uint8_t* pkt = new uint8_t[IP_MAXPACKET];

    if (!pkt) {
        close (sd);
        throw exceptionLevel("Cannot allocate memory for array 'pkt'.", true);
    }

    fd_set fdset;
    struct timeval tv;
    FD_ZERO(&fdset);
    FD_SET(sd, &fdset);
    tv.tv_sec = this->_timeout;
    tv.tv_usec = 0;

    status = select(sd + 1, &fdset, 0, 0, &tv);

    if (status == -1) {
        delete[] pkt;
        close (sd);
        throw exceptionLevel("select", true);
    } else if (status == 0) {
        delete[] pkt;
        close (sd);
        throw pingException("select timed out\n");
    }

    if ( recvfrom (sd, (void*)pkt, sizeof(struct ip) + sizeof(struct icmp) + datalen , 0, NULL, (socklen_t*)sizeof(struct sockaddr)) < 0 )  {
        delete[] pkt;
        close (sd);
        throw pingException("recvfrom() failed");
    }

    struct ip* ip = (struct ip*)pkt;

    struct icmp* icmp = (struct icmp*)(pkt + sizeof(struct ip));

    if (icmp->icmp_type == ICMP_ECHOREPLY) {
        // check that everything matches
        uint8_t* pPktData = (uint8_t*)icmp->icmp_dun.id_data;

        if (memcmp(data, pPktData, datalen)) {
            delete[] pkt;
            close (sd);
            throw pingException("data mismatch in returned packet");
        } else if (icmp->icmp_hun.ih_idseq.icd_seq != htons(msgSeq)) {
            delete[] pkt;
            close (sd);
            throw pingException("seq# mismatch in returned packet");
        }

        char srcIP[20];
        char destIP[20];
        strncpy(srcIP, (char*)inet_ntoa(*(struct in_addr*)&ip->ip_src), sizeof(srcIP));
        strncpy(destIP, (char*)inet_ntoa(*(struct in_addr*)&ip->ip_dst), sizeof(destIP));
        syslog(LOG_INFO, "%s %s %d OK\n", destIP, srcIP,  icmp->icmp_type);
    } else if (icmp->icmp_type == ICMP_UNREACH) {
        delete[] pkt;
        close (sd);
        string str = string(inet_ntoa(*(struct in_addr*)&ip->ip_dst)) + string(" is unreachable.");
        throw pingException(str);
    } else {
        delete[] pkt;
        close (sd);
        throw pingException("unknown icmp type returned.");
    }

    delete[] pkt;
    pkt = 0;

    // Close socket descriptor.
    close (sd);
}

void Ping::pingGateway ()
{

    // ICMP data
    int i;
    uint32_t r;

    for (i = 0; i < datalen_; i += sizeof(r)) {
        r = getRandom32();
        memcpy(&data_[i], &r, sizeof(r));
    }

    try {
        seqNo_++;
        this->ping(rtInfo_.gwAddr, rtInfo_.srcAddr, rtInfo_.ifIndex, data_, datalen_, seqNo_);
        state_ = OK;
        failCount_ = 0;
    } catch (pingException& e) {
        if (++failCount_ > allowedFailCount_) {
            state_ = Fail;
        } else {
            state_ = Retry;
        }

        throw e;
    } catch (exception& e) {
        state_ = Fail;
        throw e;
    } catch (...) {
        throw runtime_error("ping");
    }
}

Ping::Ping(int datalen, int timeout) :
    datalen_(datalen),
    timeout_(timeout),
    state_(Unknown),
    failCount_(0),
    allowedFailCount_(0),
    seqNo_(0)
{
    // make packet data large enough to hold a whole number of 32-bit numbers.
    try {
        data_ = new uint8_t[datalen_ + 1 + sizeof(uint32_t)];
    } catch (exception& e) {
        throw e;
    } catch (...) {
        throw bad_alloc();
    }

    memset(&rtInfo_, 0, sizeof(rtInfo_));

    try {
        getRouteInfo(&_rtInfo);
    } catch (exception& e) {
        delete data_;
        throw;
    } catch (...) {
        delete data_;
        throw runtime_error("unable to get route info");
    }
}

Ping::~Ping()
{
    delete data_;
}


