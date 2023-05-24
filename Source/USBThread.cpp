/*
------------------------------------------------------------------

This file is part of the Open Ephys GUI
Copyright (C) 2016 Open Ephys

------------------------------------------------------------------

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "USBThread.h"
#include "rhythm-api/rhd2000evalboardusb3.h"

using namespace RhythmNode;

USBThread::USBThread(Rhd2000EvalBoardUsb3* b)
	: m_board(b), Thread("USBThread")
{
}


USBThread::~USBThread()
{
}

void USBThread::startAcquisition(int nBytes)
{
    ScopedLock lock(m_lock);
	for (int i = 0; i < 2; i++)
	{
		m_lastRead[i] = 0;
		m_buffers[i].malloc(nBytes);
	}
	m_curBuffer = 0;
	m_readBuffer = 0;
	m_canRead = true;
    has_errored = false;
	startThread();
}

void USBThread::stopAcquisition()
{
	std::cout << "Stopping usb thread" << std::endl;
	if (isThreadRunning())
	{
		if (!stopThread(1000))
		{
			std::cerr << "USB Thread could not stop cleanly. Force quitting it" << std::endl;
		}
	}
}

long USBThread::usbRead(unsigned char*& buffer)
{
	ScopedLock lock(m_lock);
	if (m_readBuffer == m_curBuffer)
		return 0;
	buffer = m_buffers[m_readBuffer].getData();
	long read = m_lastRead[m_readBuffer];
	m_readBuffer = ++m_readBuffer % 2;
    if (read < 0) {
        m_canRead = false;
    } else {
        m_canRead = true;
    }
	notify();
	return read;
}

bool USBThread::hasErrored() {
    return has_errored;
}

void USBThread::run()
{
	while (!threadShouldExit())
	{
		m_lock.enter();
		if (m_canRead)
		{
			m_lock.exit();

            int num_continuous_zeros = 0;
            auto first_zero_at = std::chrono::steady_clock::now();

            long read = 0;
            while (true) {
                if (threadShouldExit()) {
                    break;
                }

                read = m_board->readDataBlocksRaw(1, m_buffers[m_curBuffer].getData());
                if (read == 0) {
                    if (num_continuous_zeros == 0) {
                        first_zero_at = std::chrono::steady_clock::now();
                    }
                    num_continuous_zeros++;
                    // After some number of consecutive zeros and a timeout, bail.
                    if (num_continuous_zeros >= 100 && std::chrono::steady_clock::now() - first_zero_at > std::chrono::seconds(5)) {
                        // ERROR
                        LOGE("Board seems to have stopped producing data for >= 5s. Bailing.");
                        has_errored = true;
                        read = -1;
                        signalThreadShouldExit();
                        break;
                    }
                    continue;
                } else if (read > 0) {
                    num_continuous_zeros = 0;
                    break;
                } else {
                    // ERROR
                    LOGE("Error occurred when reading from Rhythm USB. Return code=", read);
                    // read will be set as negative and thus m_lastRead will have a negative return code, which is
                    // respected above
                    has_errored = true;
                    signalThreadShouldExit();
                    break;
                }
            }

			{
				ScopedLock lock(m_lock);
				m_lastRead[m_curBuffer] = read;
				m_curBuffer = ++m_curBuffer % 2;
				m_canRead = false;
			}
		}
		else
			m_lock.exit();

		if (!threadShouldExit())
			wait(100);
	}
}
