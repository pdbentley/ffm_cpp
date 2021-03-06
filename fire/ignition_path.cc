#include <string>

#include "ffm_util.h"
#include "ignition_path.h"


/*!\brief Categorises a fire as spreading or not. 

  \return true if and only if the rate of spread is at least
  ffm_settings::minRateForStratumSpread for at least ffm_settings::minTimeStepsForStratumSpread 
  time steps, otherwise returns false.
*/
bool IgnitionPath::spreads() const {
  int count = 0;
  for (int i = 0; i < numSegments(); ++i){
    if (ros(i) > ffm_settings::minRateForStratumSpread) 
      ++count;
    if (count >= ffm_settings::minTimeStepsForStratumSpread) return true;
  }
  return false;
}

/*!\brief Mean flame length from all segments.
  \return The mean flame length from all segments of the IgnitionPath. 
  If ignoreZeros == true the only non-zero flame lengths will be used to 
  compute the mean, otherwise all flame lengths will be used
*/
double IgnitionPath::meanFlameLength(const bool& ignoreZeros) const {
  if (!hasSegments()) return 0.0;
  std::vector<double> vals;
  vals.resize(numSegments());
  transform(ignitedSegments_.begin(), ignitedSegments_.end(), vals.begin(), 
            [this](const Seg& s){return species().flameLength(s.length());});
  return ffm_util::mean(vals, ignoreZeros);
}


/*!\brief Standard deviation of flame lengths from all segments.
  \return The standard deviation of the flame lengths from all segments of the IgnitionPath. 
  If ignoreZeros == true the only non-zero flame lengths will be used to 
  compute the standard deviation, otherwise all flame lengths will be used
*/
double IgnitionPath::stdDevFlameLength(const bool& ignoreZeros) const {
  if (numSegments() < 2) return 0.0;
  std::vector<double> vals;
  vals.resize(numSegments());
  transform(ignitedSegments_.begin(), ignitedSegments_.end(), vals.begin(), 
            [this](const Seg& s){return species().flameLength(s.length());});
  return ffm_util::stdDev(vals, ignoreZeros);
}


/*!\brief The FlameSeries associated with the IgnitionPath
  \param windSpeed The relevant wind speed used for computation of the flame angles
  \param slope
  \return The FlameSeries generated by the IgnitionPath.
*/
FlameSeries IgnitionPath::flameSeries(const double& windSpeed, 
                                      const double& slope) const {
  FlameSeries retValue(level_);
  if (hasSegments()) {
    retValue.flames().reserve(numSegments());
    for (int i = 0; i < numSegments(); ++i) retValue.addFlame(flame(i, windSpeed, slope));
  }
  return retValue;
}

/*!\brief Extract a rate of spread
  \param idx
  \return The rate of spread computed at the idx-th segment, 
  counting from idx = 0.
*/
double IgnitionPath::ros(const int& idx) const {
  if (idx >= numSegments()) return 0;
  if (idx == 0) {
    Seg s = ignitedSegments_.at(0);
    return (s.end().x() - s.start().x())/ffm_settings::computationTimeInterval;
  } else 
    return (ignitedSegments_.at(idx).end().x() - ignitedSegments_.at(idx-1).end().x())/
      ffm_settings::computationTimeInterval;
}

/*!\brief Basic rate of spread calculation
  \return The average rate of spread of all segments in the IgnitionPath 
  whose rate of spread exceeds ffm_settings::minRateForStratumSpread.
*/
double IgnitionPath::basicROS() const {
  if (!hasSegments()) return 0;
  double sum = 0, x = ignitedSegments_.front().start().x();
  int count = 0;
  for (const Seg& seg : ignitedSegments_) {
    double tmp = (seg.end().x() - x)/ffm_settings::computationTimeInterval;
    if (tmp > ffm_settings::minRateForStratumSpread){
      sum += tmp;
      ++count;
    }
    x = seg.end().x();
  }
  return count == 0 ? 0 : sum / count;
}

/*!\brief Non-independent rate of spread
  \return The quotient of the total horizontal distance travelled and 
  the sum of the time to ignition and the time period covered by the IgnitionPath.
*/
double IgnitionPath::nonIndependentROS() const {
  if (!hasSegments()) return 0;
  // return (ignitedSegments_.back().end().x() - ignitedSegments_.front().start().x())/
  //   (ffm_settings::computationTimeInterval*(numSegments() + startTimeStep_));
  return (maxX() - ignitedSegments_.front().start().x())/
    (ffm_settings::computationTimeInterval*(numSegments() + startTimeStep_));
}

/*!\brief Time of spread
  \return The time in seconds for which spread by the 
  IgnitionPath exceeded ffm_settings::minRateForStratumSpread.
*/
 double IgnitionPath::timeOfSpread() const {
  if (!hasSegments()) return 0;
  int count = 0;
  for (int i = 0; i <= numSegments() - 1; ++i)
    count += ros(i) >= ffm_settings::minRateForStratumSpread ? 1 : 0;
  return count*ffm_settings::computationTimeInterval;
}

//printing 

/*!\brief Printing
  \return A formatted string describing the ignition path.
*/
std::string IgnitionPath::printToString() const{
  std::string str;
  char s[100];

  str =  "Path type:                " + ignitionPathTypeStringMap.at(type_) + "\n";
  str += "Level:                    " + levelStringMap.at(level_) + "\n";
  str += "Species:                  " + species_.name() + "\n";
  sprintf(s, "Flame duration (sec):     %.2f\n", species_.flameDuration());
  str += std::string(s);

  if (hasSegments()) {
    sprintf(s,"Ignition start time step: %d", startTimeStep_); 
    str += std::string(s);
  }
  else {
    str += "Ignition start time step: NA";
  }

  bool hasPreHeatingData = false;
  bool hasIncidentData = false;
  for (int i = 0; i < preIgnitionData_.size(); i++) {
    PreIgnitionData pid = preIgnitionData_.at(i);
    switch (pid.type()) {
      case PreIgnitionData::Type::PREHEATING: hasPreHeatingData = true; break;
      case PreIgnitionData::Type::INCIDENT: hasIncidentData = true; break;
    }
  }

  if (hasPreHeatingData) {
    str += "\n\nPreHeating\n";
    str += "Length\tDepth\tDist\tTemp\tDrying\tDuration\n";
    for (int i = 0; i < preIgnitionData_.size(); i++) {
      PreIgnitionData pid = preIgnitionData_.at(i);
      if (pid.type() == PreIgnitionData::Type::PREHEATING) {
        sprintf(s, "%.2f\t%.2f\t%.2f\t%.2f\t%.4f\t%.2f\n", 
	        pid.flameLength(),
		pid.depthIgnited(),
		pid.distanceToFlame(),
		pid.temperature(),
		pid.dryingFactor(), 
		pid.duration());
        str += std::string(s);
      }
    }
  }

  if (hasIncidentData) {
    str += "\n\nIncident\n";
    str += "Length\tDepth\tDist\tTemp\tDrying\tIDT\n";
    for (int i = 0; i < preIgnitionData_.size(); i++) {
      PreIgnitionData pid = preIgnitionData_.at(i);
      if (pid.type() != PreIgnitionData::Type::PREHEATING) {

        sprintf(s, "%.2f\t%.2f\t%.2f\t%.2f\t%.4f\t%.2f\n", 
	        pid.flameLength(),
		pid.depthIgnited(),
		pid.distanceToFlame(),
		pid.temperature(),
		pid.dryingFactor(), 
		pid.idt());
        str += std::string(s);
      }
    }
  }

  if (hasSegments()){
    str += "\n\nIgnited segments:\n";
    str += "Time\tStartX\tStartY\tEndX\tEndY\tSegLen\tFlameLen\n";

    for (int i = 0; i < numSegments(); i++) {
      Seg seg = ignitedSegments_.at(i);
      sprintf(s, "%d\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n", 
          i+1, seg.start().x(), seg.start().y(), seg.end().x(), seg.end().y(), seg.length(), species_.flameLength(seg.length()) );
      str += std::string(s);
    }
  }

  return str;
}

