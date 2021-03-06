// meteraverage.h
//
// Average sucessive levels for a meter.
//
//   (C) Copyright 2007-2015 Fred Gleason <fredg@paravelsystems.com>
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License version 2 as
//   published by the Free Software Foundation.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public
//   License along with this program; if not, write to the Free Software
//   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#ifndef METERAVERAGE_H
#define METERAVERAGE_H

#include <queue>

class MeterAverage
{
 public:
  MeterAverage(int maxsize);
  float average() const;
  void addValue(float value);

 private:
  int avg_maxsize;
  float avg_total;
  std::queue<float> avg_values;
};


#endif  // METERAVERAGE_H
