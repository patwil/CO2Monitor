/*
 * ping.h
 *
 * Created on: 2016-02-08
 *     Author: patw
 */

#ifndef PING_H
#define PING_H

#include <string>
#include <arpa/inet.h>        // inet_pton() and inet_ntop()

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
            HwFail,
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

        void setTerminateFd(int fd) { terminateFd_ = fd; }

    private:
        int datalen_;
        uint8_t* data_;
        RouteInfo_t rtInfo_;
        uint16_t seqNo_;
        int timeout_;
        static const int kDefaultDatalen_ = 56;
        static const int kDefaultTimeout_ = 5; // seconds
        int terminateFd_; // use this to abort wait for ping reply when stopping service

        State state_;
        int failCount_;
        int allowedFailCount_;
        int consecutiveHwFailCount_;

        void getRouteInfo(RouteInfo_t* pRtInfo);
        void printRouteInfo(RouteInfo_t* pRtInfo);
        uint16_t checksum (void* addr, int len);
        uint32_t getRandom32();
        int readNlSock(int sockFd, uint8_t* bufPtr, uint32_t seqNum, uint32_t pId);
        void parseRouteInfo(struct nlmsghdr* nlHdr, RouteInfo_t* pRtInfo);
        void ping(in_addr_t destAddr, in_addr_t srcAddr, uint32_t ifIndex, uint8_t* data, uint32_t datalen, uint16_t msgSeq);

};


#endif /* PING_H */
