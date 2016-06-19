/*
 * co2Display.cpp
 *
 * Created on: 2016-06-18
 *     Author: patw
 */

#include "co2Display.h"

using namespace std;

Co2Display::Co2Display()
{
}

Co2Display::Co2Display(const Co2Display& rhs)
{
    // Assign new dynamic memory and and copy over data.

}


Co2Display::~Co2Display()
{
    // Delete all dynamic memory.
}

Co2Display& Co2Display::operator=(const Co2Display& rhs)
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
//	The system will always provide one so make this private if you don't
//  want it. Remove the function if the default is ok.
// Used:
//	  Co2Display *c1 = &c2;

Co2Display* Co2Display::operator&()
{
    return (this);
}

// Address of operator (const).
//	The system will always provide one so make this private if you don't
//  want it. Remove the function if the default is ok.
// Used:
//	  const Co2Display *c1 = &c2;

const Co2Display* Co2Display::operator&() const
{
    return (this);
}


// The following operators can be declared:
// +   -   *   /   %   ^   &   |   ~   !
// =   <   >   +=  -=  *=  /=  %=  ^=  &=
// |=  <<  >>  >>= <<= ==  !=  <=  >=  &&
// ||  ++  --  ->* ,   ->  []  ()  new delete
// NB: Co2Display operator++ () is pre-increment.
//     Co2Display operator++ (int) is post-increment.
// There should be no public data members.


istream& operator>>(istream& in, Co2Display co2Display)
{
    // Extract the data from the stream. eg:
    // char c;
    // try {
    //     if ( in>>c && (c == '{') ) {
    //         co2Display.setChar(c);
    //     }
    // }
    // catch (...) {
    //     in.setstate(ios_base::failbit); // register the failure in the stream
    // }
    //
    return in;
}

ostream& operator<<(ostream& out, Co2Display co2Display)
{
    // Extract the data from the object. eg:
    // return out << co2Display.printable();
    return out;
}

