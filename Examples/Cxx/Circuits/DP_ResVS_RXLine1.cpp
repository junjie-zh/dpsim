/** Reference Circuits
 *
 * @author Markus Mirz <mmirz@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *
 * DPsim
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include "Simulation.h"
#include "Utilities.h"

using namespace DPsim;

int main(int argc, char* argv[])
{
	// Define simulation scenario
	Real timeStep = 0.001;
	Real omega = 2.0*M_PI*50.0;
	Real finalTime = 0.3;
	String simName = "RxLineResLoad_" + std::to_string(timeStep);
	Components::Base::List circElements0, circElements1, circElements2;
	circElements0.push_back(std::make_shared<Components::DP::VoltageSource>("v_s", 1, 0, Complex(10000, 0), 1));
	circElements0.push_back(std::make_shared<Components::DP::Resistor>("r_line", 1, 2, 1));
	circElements0.push_back(std::make_shared<Components::DP::Inductor>("l_line", 2, 3, 1));
	circElements1 = circElements0;
	circElements2 = circElements0;
	circElements1.push_back(std::make_shared<Components::DP::Resistor>("r_load", 3, 0, 1000));
	circElements2.push_back(std::make_shared<Components::DP::Resistor>("r_load", 3, 0, 800));

	// Set up simulation and start main simulation loop
	Simulation newSim(simName, circElements1, omega, timeStep, finalTime);
	newSim.addSystemTopology(circElements2);
	newSim.setSwitchTime(0.1, 1);

	std::cout << "Start simulation." << std::endl;

	while (newSim.step()) {
		newSim.increaseByTimeStep();
	}

	std::cout << "Simulation finished." << std::endl;

	return 0;
}