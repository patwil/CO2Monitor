/*
 * co2Monitor.cpp
 *
 * Created on: 2016-02-13
 *     Author: patw
 */

#include "co2Monitor.h"
#include "co2Message.pb.h"
#include <google/protobuf/text_format.h>

Co2Monitor::Co2Monitor()
{
}

Co2Monitor::Co2Monitor(const Co2Monitor& rhs)
{
    // Assign new dynamic memory and and copy over data.

}


Co2Monitor::~Co2Monitor()
{
    // Delete all dynamic memory.
}

Co2Monitor& Co2Monitor::operator=(const Co2Monitor& rhs)
{
    // Don't copy to self.
    if (this != &rhs) {
        // Make sure that the base class assigns too.
        // BaseClass::operator= (rhs);

        // Delete existing memory from this and assign new.
        // Copy data from (non-NULL) pointers into new memory.
        // Assign all data members.

    }

    //
    return *this;
}

// Address of operator (non const).
//  The system will always provide one so make this private if you don't
//  want it. Remove the function if the default is ok.
// Used:
//    Co2Monitor *c1 = &c2;

Co2Monitor* Co2Monitor::operator&()
{
    return (this);
}

// Address of operator (const).
//  The system will always provide one so make this private if you don't
//  want it. Remove the function if the default is ok.
// Used:
//    const Co2Monitor *c1 = &c2;

const Co2Monitor* Co2Monitor::operator&() const
{
    return (this);
}


// The following operators can be declared:
// +   -   *   /   %   ^   &   |   ~   !
// =   <   >   +=  -=  *=  /=  %=  ^=  &=
// |=  <<  >>  >>= <<= ==  !=  <=  >=  &&
// ||  ++  --  ->* ,   ->  []  ()  new delete
// NB: Co2Monitor operator++ () is pre-increment.
//     Co2Monitor operator++ (int) is post-increment.
// There should be no public data members.


istream& operator>>(istream& in, Co2Monitor co2Monitor)
{
    // Extract the data from the stream. eg:
    // char c;
    // try {
    //     if ( in>>c && (c == '{') ) {
    //         co2Monitor.setChar(c);
    //     }
    // }
    // catch (...) {
    //     in.setstate(ios_base::failbit); // register the failure in the stream
    // }
    //
    return in;
}

ostream& operator<<(ostream& out, Co2Monitor co2Monitor)
{
    // Extract the data from the object. eg:
    // return out << co2Monitor.printable();
    return out;
}

