/*
 * restartMgr.h
 *
 * Created on: 2016-07-07
 *     Author: patw
 */

#ifndef RESTARTMGR_H
#define RESTARTMGR_H

#include <iostream>

class RestartMgr
{
    public:
        RestartMgr();

        virtual ~RestartMgr();


    private:
        RestartMgr(const RestartMgr& rhs);
        RestartMgr& operator=(const RestartMgr& rhs);
        RestartMgr* operator&();
        const RestartMgr* operator&() const;

        void doShutDown(bool bReboot);

    protected:
};

#endif /* RESTARTMGR_H */
