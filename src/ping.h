/*
 * ping.h
 *
 * Created on: 2016-02-08
 *     Author: patw
 */

#ifndef PING_H
#define PING_H

#include <iostream>
#include <typeinfo>
#include <exception>
#include <string>
#include <cstdlib>
#include <syslog.h>
#include <unistd.h>           // close()
#include <cstring>           // strcpy, memset(), and memcpy()
#include <sys/time.h>
#include <netdb.h>            // struct addrinfo
#include <sys/types.h>        // needed for socket()
#include <sys/socket.h>       // needed for socket()
#include <netinet/in.h>       // IPPROTO_RAW, IPPROTO_IP, IPPROTO_ICMP
#include <netinet/ip.h>       // struct ip and IP_MAXPACKET (which is 65535)
#include <netinet/ip_icmp.h>  // struct icmp, ICMP_ECHO
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <sys/ioctl.h>        // macro ioctl is defined
#include <net/if.h>           // struct ifreq
#include <asm/types.h>
#include <netinet/ether.h>
#include <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <cerrno>            // errno, perror()

typedef struct {
    in_addr_t destAddr;
    in_addr_t srcAddr;
    in_addr_t gwAddr;
    uint32_t  ifIndex;
} RouteInfo_t;


class pingException: public std::exception
{
        std::string errorStr_;
    public:
    pingException(const std::string errorStr = "ping exception") noexcept :
        errorStr_(errorStr) {}

        virtual const char* what() const throw() {
            return errorStr_.c_str();
        }
};

class Ping
{
    public:
        Ping(int datalen = kDefaultDatalen_, int timeout = kDefaultTimeout_);

        ~Ping();

        void pingGateway();

        typedef enum {
            OK,
            Fail,
            Retry,
            Unknown
        } State;

        State state() {
            return state_;
        }

        int setAllowedFailCount(int allowedFailCount) {
            if (allowedFailCount == allowedFailCount_) {
                return allowedFailCount_;
            }

            int oldAllowedFailCount = allowedFailCount_;
            allowedFailCount_ = allowedFailCount;
            return oldAllowedFailCount;
        }

        int allowedFailCount() {
            return allowedFailCount_;
        }

    private:
        int datalen_;
        uint8_t* data_;
        RouteInfo_t rtInfo_;
        uint16_t seqNo_;
        int timeout_;
        static const int kDefaultDatalen_ = 56;
        static const int kDefaultTimeout_ = 5; // seconds

        State state_;
        int failCount_;
        int allowedFailCount_;

        void getRouteInfo(RouteInfo_t* pRtInfo);
        void printRouteInfo(RouteInfo_t* pRtInfo);
        uint16_t checksum (void* addr, int len);
        uint32_t getRandom32();
        int readNlSock(int sockFd, uint8_t* bufPtr, uint32_t seqNum, uint32_t pId);
        void parseRouteInfo(struct nlmsghdr* nlHdr, RouteInfo_t* pRtInfo);
        void ping(in_addr_t destAddr, in_addr_t srcAddr, uint32_t ifIndex, uint8_t* data, uint32_t datalen, uint16_t msgSeq);

};


#endif /* PING_H */
