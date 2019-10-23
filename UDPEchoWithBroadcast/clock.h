

#pragma once

#ifndef _CLOCK_H_
#define _CLOCK_H_

// Library Includes
#include <chrono>
#include <vector>


// Prototypes
class CClock
{
	// Member Functions
public:
	CClock();
	~CClock();
	bool Initialise();
	void Process();
	double GetDeltaTick() const;
	double GetTimeElapsedMS() const;
	double GetTimeElapsedS() const;
	void ResetElaspedTime() { m_fTimeElapsed = 0.0; }

private:
	double m_fTimeElapsed;
	double m_fDeltaTime;
	std::chrono::high_resolution_clock::time_point m_fLastTime;
	std::chrono::high_resolution_clock::time_point m_fCurrentTime;
};
#endif // 
