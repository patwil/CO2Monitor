/*
 * displayElement.cpp
 *
 * Created on: 2016-06-18
 *     Author: patw
 */

#include "displayElement.h"

DisplayElement::DisplayElement()
{
}

DisplayElement::DisplayElement(const DisplayElement& rhs)
{
    // Assign new dynamic memory and and copy over data.

}


DisplayElement::~DisplayElement()
{
    // Delete all dynamic memory.
}

DisplayElement& DisplayElement::operator=(const DisplayElement& rhs)
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
//    DisplayElement *c1 = &c2;

DisplayElement* DisplayElement::operator&()
{
    return (this);
}

// Address of operator (const).
//  The system will always provide one so make this private if you don't
//  want it. Remove the function if the default is ok.
// Used:
//    const DisplayElement *c1 = &c2;

const DisplayElement* DisplayElement::operator&() const
{
    return (this);
}


// The following operators can be declared:
// +   -   *   /   %   ^   &   |   ~   !
// =   <   >   +=  -=  *=  /=  %=  ^=  &=
// |=  <<  >>  >>= <<= ==  !=  <=  >=  &&
// ||  ++  --  ->* ,   ->  []  ()  new delete
// NB: DisplayElement operator++ () is pre-increment.
//     DisplayElement operator++ (int) is post-increment.
// There should be no public data members.


istream& operator>>(istream& in, DisplayElement displayElement)
{
    // Extract the data from the stream. eg:
    // char c;
    // try {
    //     if ( in>>c && (c == '{') ) {
    //         displayElement.setChar(c);
    //     }
    // }
    // catch (...) {
    //     in.setstate(ios_base::failbit); // register the failure in the stream
    // }
    //
    return in;
}

ostream& operator<<(ostream& out, DisplayElement displayElement)
{
    // Extract the data from the object. eg:
    // return out << displayElement.printable();
    return out;
}

