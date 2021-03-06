/*
*   Copyright (C) 2016,2017,2018 by Jonathan Naylor G4KLX
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "NXDNLookup.h"
#include "Timer.h"
#include "Log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

CNXDNLookup::CNXDNLookup(const std::string& filename, unsigned int reloadTime) :
CThread(),
m_filename(filename),
m_reloadTime(reloadTime),
m_table(),
m_cstable(),
m_mutex(),
m_stop(false)
{
}

CNXDNLookup::~CNXDNLookup()
{
}

bool CNXDNLookup::read()
{
	bool ret = load();

	if (m_reloadTime > 0U)
		run();

	return ret;
}

void CNXDNLookup::entry()
{
	LogInfo("Started the NXDN Id lookup reload thread");

	CTimer timer(1U, 3600U * m_reloadTime);
	timer.start();

	while (!m_stop) {
		sleep(1000U);

		timer.clock();
		if (timer.hasExpired()) {
			load();
			timer.start();
		}
	}

	LogInfo("Stopped the NXDN Id lookup reload thread");
}

void CNXDNLookup::stop()
{
	if (m_reloadTime == 0U) {
		delete this;
		return;
	}

	m_stop = true;

	wait();
}

std::string CNXDNLookup::findCS(unsigned int id)
{
	std::string callsign;

	if (id == 0xFFFFU)
		return std::string("ALL");

	m_mutex.lock();

	try {
		callsign = m_table.at(id);
	} catch (...) {
		char text[10U];
		::sprintf(text, "%u", id);
		callsign = std::string(text);
	}

	m_mutex.unlock();

	return callsign;
}

unsigned int CNXDNLookup::findID(std::string cs)
{
	unsigned int nxdnID;

	m_mutex.lock();

	try {
		nxdnID = m_cstable.at(cs);
	} catch (...) {
		nxdnID = 0U;
	}

	m_mutex.unlock();

	return nxdnID;
}

bool CNXDNLookup::exists(unsigned int id)
{
	m_mutex.lock();

	bool found = m_table.count(id) == 1U;

	m_mutex.unlock();

	return found;
}

bool CNXDNLookup::load()
{
	FILE* fp = ::fopen(m_filename.c_str(), "rt");
	if (fp == NULL) {
		LogWarning("Cannot open the NXDN Id lookup file - %s", m_filename.c_str());
		return false;
	}

	m_mutex.lock();

	// Remove the old entries
	m_table.clear();
	m_cstable.clear();

	char buffer[100U];
	while (::fgets(buffer, 100U, fp) != NULL) {
		if (buffer[0U] == '#')
			continue;

		char* p1 = ::strtok(buffer, ",\t\r\n");
		char* p2 = ::strtok(NULL, ",\t\r\n");

		if (p1 != NULL && p2 != NULL) {
			unsigned int id = (unsigned int)::atoi(p1);
			if (id > 0U) {
				for (char* p = p2; *p != 0x00U; p++)
					*p = ::toupper(*p);

				m_table[id] = std::string(p2);
				m_cstable[p2] = id;
			}
		}
	}

	m_mutex.unlock();

	::fclose(fp);

	size_t size = m_table.size();
	if (size == 0U)
		return false;

	LogInfo("Loaded %u Ids to the NXDN callsign lookup table", size);

	return true;
}