/*
 * co2Display.h
 *
 * Created on: 2016-06-18
 *     Author: patw
 */

#ifndef CO2DISPLAY_H
#define CO2DISPLAY_H

#include <iostream>

class Co2Display
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
        //	  Co2Display c1;
        Co2Display();

        // Copy constructor.
        //	The system will always provide one so make this private if you don't
        //  want it. Remove the function if the default is ok.
        //	You will need to provide one if dynamic memory is used.
        //	Used:
        //	  Co2Display c1(c2);
        //	  pass by value to function.
        //	  return value from function

        Co2Display(const Co2Display& rhs);

        // Destructor
        //	The system will always provide one so make this private if you don't
        //  want it. Remove the function if the default is ok.
        //	You will need to provide one if dynamic memory is used.
        // This should be virtual for Base classes otherwise subclass destructors will not be used.

        virtual ~Co2Display();

        // Operators.
        // Assignment operator.
        //	The system will always provide one so make this private if you don't
        //  want it. Remove the function if the default is ok.
        //	You will need to provide one if dynamic memory is used.

        Co2Display& operator=(const Co2Display& rhs);

        // Address of operator (non const).
        //	The system will always provide one so make this private if you don't
        //  want it. Remove the function if the default is ok.
        // Used:
        //	  Co2Display *c1 = &c2;

        Co2Display* operator&();

        // Address of operator (const).
        //	The system will always provide one so make this private if you don't
        //  want it. Remove the function if the default is ok.
        // Used:
        //	  const Co2Display *c1 = &c2;

        const Co2Display* operator&() const;

        // The following operators can be declared:
        // +   -   *   /   %   ^   &   |   ~   !
        // =   <   >   +=  -=  *=  /=  %=  ^=  &=
        // |=  <<  >>  >>= <<= ==  !=  <=  >=  &&
        // ||  ++  --  ->* ,   ->  []  ()  new delete
        // NB: Co2Display operator++ () is pre-increment.
        //     Co2Display operator++ (int) is post-increment.
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

std::istream& operator>>(std::istream& in, Co2Display co2Display);

// Output
//	This is never a class member.
//		Maybe a friend.
//		Maybe use an access function.

std::ostream& operator<<(std::ostream& out, Co2Display co2Display);

#endif /* CO2DISPLAY_H */
