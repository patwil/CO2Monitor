/*
 * displayElement.h
 *
 * Created on: 2016-06-18
 *     Author: patw
 */

#ifndef DISPLAYELEMENT_H
#define DISPLAYELEMENT_H

#include <iostream>

class DisplayElement
{
    public:
        // Constructors.
        //   Initialisation List
        //	   initialise all pointers (to zero).
        //	   list members in order declared in class.

        // Default Constructor
        //	The system will always provide one so make this private if you don't
        //  want it. Remove the function if the default is ok.
        //	Used:
        //	  DisplayElement c1;
        DisplayElement();

        // Copy constructor.
        //	The system will always provide one so make this private if you don't
        //  want it. Remove the function if the default is ok.
        //	You will need to provide one if dynamic memory is used.
        //	Used:
        //	  DisplayElement c1(c2);
        //	  pass by value to function.
        //	  return value from function

        DisplayElement(const DisplayElement& rhs);

        // Destructor
        //	The system will always provide one so make this private if you don't
        //  want it. Remove the function if the default is ok.
        //	You will need to provide one if dynamic memory is used.
        // This should be virtual for Base classes otherwise subclass destructors will not be used.

        virtual ~DisplayElement();

        // Operators.
        // Assignment operator.
        //	The system will always provide one so make this private if you don't
        //  want it. Remove the function if the default is ok.
        //	You will need to provide one if dynamic memory is used.

        DisplayElement& operator=(const DisplayElement& rhs);

        // Address of operator (non const).
        //	The system will always provide one so make this private if you don't
        //  want it. Remove the function if the default is ok.
        // Used:
        //	  DisplayElement *c1 = &c2;

        DisplayElement* operator&();

        // Address of operator (const).
        //	The system will always provide one so make this private if you don't
        //  want it. Remove the function if the default is ok.
        // Used:
        //	  const DisplayElement *c1 = &c2;

        const DisplayElement* operator&() const;

        // The following operators can be declared:
        // +   -   *   /   %   ^   &   |   ~   !
        // =   <   >   +=  -=  *=  /=  %=  ^=  &=
        // |=  <<  >>  >>= <<= ==  !=  <=  >=  &&
        // ||  ++  --  ->* ,   ->  []  ()  new delete
        // NB: DisplayElement operator++ () is pre-increment.
        //     DisplayElement operator++ (int) is post-increment.
        // There should be no public data members.

    private:

    protected:
};


// I/O
//	Declare these in header file.
//	Define them in a source file
//
// Input
//	This is never a class member.
//		Maybe a friend.
//		Maybe use an access function.

std::istream& operator>>(std::istream& in, DisplayElement displayElement);

// Output
//	This is never a class member.
//		Maybe a friend.
//		Maybe use an access function.

std::ostream& operator<<(std::ostream& out, DisplayElement displayElement);

#endif /* DISPLAYELEMENT_H */
