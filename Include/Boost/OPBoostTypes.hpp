#pragma once
#include <boost/format.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"

namespace BSTPosixTime = boost::posix_time;
using BSTTime = BSTPosixTime::ptime;
using BSTSecondClock = BSTPosixTime::second_clock;

using BSTFormat = boost::format;
