/*
 * co2Monitor.h
 *
 * Created on: 2016-02-13
 *     Author: patw
 */

#ifndef CO2MONITOR_H
#define CO2MONITOR_H

#include <iostream>

class Co2Monitor
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
        //	  Co2Monitor c1;
        Co2Monitor();

        // Copy constructor.
        //	The system will always provide one so make this private if you don't
        //  want it. Remove the function if the default is ok.
        //	You will need to provide one if dynamic memory is used.
        //	Used:
        //	  Co2Monitor c1(c2);
        //	  pass by value to function.
        //	  return value from function

        Co2Monitor(const Co2Monitor& rhs);

        // Destructor
        //	The system will always provide one so make this private if you don't
        //  want it. Remove the function if the default is ok.
        //	You will need to provide one if dynamic memory is used.
        // This should be virtual for Base classes otherwise subclass destructors will not be used.

        virtual ~Co2Monitor();

        // Operators.
        // Assignment operator.
        //	The system will always provide one so make this private if you don't
        //  want it. Remove the function if the default is ok.
        //	You will need to provide one if dynamic memory is used.

        Co2Monitor& operator=(const Co2Monitor& rhs);

        // Address of operator (non const).
        //	The system will always provide one so make this private if you don't
        //  want it. Remove the function if the default is ok.
        // Used:
        //	  Co2Monitor *c1 = &c2;

        Co2Monitor* operator&();

        // Address of operator (const).
        //	The system will always provide one so make this private if you don't
        //  want it. Remove the function if the default is ok.
        // Used:
        //	  const Co2Monitor *c1 = &c2;

        const Co2Monitor* operator&() const;

        // The following operators can be declared:
        // +   -   *   /   %   ^   &   |   ~   !
        // =   <   >   +=  -=  *=  /=  %=  ^=  &=
        // |=  <<  >>  >>= <<= ==  !=  <=  >=  &&
        // ||  ++  --  ->* ,   ->  []  ()  new delete
        // NB: Co2Monitor operator++ () is pre-increment.
        //     Co2Monitor operator++ (int) is post-increment.
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

std::istream& operator>>(std::istream& in, Co2Monitor co2Monitor);

// Output
//	This is never a class member.
//		Maybe a friend.
//		Maybe use an access function.

std::ostream& operator<<(std::ostream& out, Co2Monitor co2Monitor);

#endif /* CO2MONITOR_H */
