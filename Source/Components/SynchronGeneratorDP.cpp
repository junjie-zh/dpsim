/** Synchron generator
 *
 * @author Markus Mirz <mmirz@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
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

#include "SynchronGeneratorDP.h"
#include "IntegrationMethod.h"

using namespace DPsim;

SynchronGenerator::SynchronGenerator(std::string name, Int node1, Int node2, Int node3,
	Real nomPower, Real nomVolt, Real nomFreq, Int poleNumber, Real nomFieldCur,
	Real Rs, Real Ll, Real Lmd, Real Lmd0, Real Lmq, Real Lmq0,
	Real Rfd, Real Llfd, Real Rkd, Real Llkd,
	Real Rkq1, Real Llkq1, Real Rkq2, Real Llkq2,
	Real inertia, bool logActive)
	: BaseComponent(name, node1, node2, node3, logActive) {

	this->mNode1 = node1 - 1;
	this->mNode2 = node2 - 1;
	this->mNode3 = node3 - 1;

	mNomPower = nomPower;
	mNomVolt = nomVolt;
	mNomFreq = nomFreq;
	mPoleNumber = poleNumber;
	mNomFieldCur = nomFieldCur;

	// base stator values
	mBase_V_RMS = mNomVolt / sqrt(3);
	mBase_v = mBase_V_RMS * sqrt(2);
	mBase_I_RMS = mNomPower / (3 * mBase_V_RMS);
	mBase_i = mBase_I_RMS * sqrt(2);
	mBase_Z = mBase_v / mBase_i;
	mBase_OmElec = 2 * DPS_PI * mNomFreq;
	mBase_OmMech = mBase_OmElec / (mPoleNumber / 2);
	mBase_L = mBase_Z / mBase_OmElec;
	mBase_Psi = mBase_L * mBase_i;
	mBase_T = mNomPower / mBase_OmMech;

	// Create logging file
	if (mLogActive) {
		std::string filename = "SynGen_" + mName + ".csv";
		mLog = new Logger(filename);
	}

	initWithPerUnitParam(Rs, Ll, Lmd, Lmd0, Lmq, Lmq0, Rfd, Llfd, Rkd, Llkd, Rkq1, Llkq1, Rkq2, Llkq2, inertia);

}

SynchronGenerator::~SynchronGenerator() {
	if (mLogActive) {
		delete mLog;
	}
}

void SynchronGenerator::initWithPerUnitParam(
	Real Rs, Real Ll, Real Lmd, Real Lmd0, Real Lmq, Real Lmq0,
	Real Rfd, Real Llfd, Real Rkd, Real Llkd,
	Real Rkq1, Real Llkq1, Real Rkq2, Real Llkq2,
	Real H) {

	if (Rkq2 == 0 && Llkq2 == 0)
	{
		DampingWindings = 1;
		mVoltages2 = Matrix::Zero(6, 1);
		mFluxes2 = Matrix::Zero(6, 1);
		mCurrents2 = Matrix::Zero(6, 1);
	}
	else
	{
		DampingWindings = 2;
	}

	// base rotor values
	mBase_ifd = Lmd * mNomFieldCur;
	mBase_vfd = mNomPower / mBase_ifd;
	mBase_Zfd = mBase_vfd / mBase_ifd;
	mBase_Lfd = mBase_Zfd / mBase_OmElec;

	mRs = Rs;
	mLl = Ll;
	mLmd = Lmd;
	mLmd0 = Lmd0;
	mLmq = Lmq;
	mLmq0 = Lmq0;
	mRfd = Rfd;
	mLlfd = Llfd;
	mRkd = Rkd;
	mLlkd = Llkd;
	mRkq1 = Rkq1;
	mLlkq1 = Llkq1;
	mRkq2 = Rkq2;
	mLlkq2 = Llkq2;
	mH = H;
	// Additional inductances according to Krause
	mLaq = 1 / (1 / mLmq + 1 / mLl + 1 / mLlkq1 + 1 / mLlkq2);
	mLad = 1 / (1 / mLmd + 1 / mLl + 1 / mLlkd + 1 / mLlfd);

	// Determinant of Ld (inductance matrix of d axis)
	detLd = (mLmd + mLl)*(-mLlfd*mLlkd - mLlfd*mLmd - mLmd*mLlkd) + mLmd*mLmd*(mLlfd + mLlkd);
	// Determinant of Lq (inductance matrix of q axis)
	if (DampingWindings == 2)
		detLq = -mLmq*mLlkq2*(mLlkq1 + mLl) - mLl*mLlkq1*(mLlkq2 + mLmq);
	else
		detLq = -(mLl + mLmq)*(mLlkq1 + mLmq) + mLmq*mLmq;
}

void SynchronGenerator::init(Real om, Real dt,
	Real initActivePower, Real initReactivePower, Real initTerminalVolt,
	Real initVoltAngle, Real initFieldVoltage, Real initMechPower) {

	// Create matrices for state space representation
	if (DampingWindings == 2)
	{
		mInductanceMat <<
			-(mLl + mLmq), 0, 0, mLmq, mLmq, 0, 0,
			0, -(mLl + mLmd), 0, 0, 0, mLmd, mLmd,
			0, 0, -mLl, 0, 0, 0, 0,
			-mLmq, 0, 0, mLlkq1 + mLmq, mLmq, 0, 0,
			-mLmq, 0, 0, mLmq, mLlkq2 + mLmq, 0, 0,
			0, -mLmd, 0, 0, 0, mLlfd + mLmd, mLmd,
			0, -mLmd, 0, 0, 0, mLmd, mLlkd + mLmd;

		mResistanceMat <<
			mRs, 0, 0, 0, 0, 0, 0,
			0, mRs, 0, 0, 0, 0, 0,
			0, 0, mRs, 0, 0, 0, 0,
			0, 0, 0, -mRkq1, 0, 0, 0,
			0, 0, 0, 0, -mRkq2, 0, 0,
			0, 0, 0, 0, 0, -mRfd, 0,
			0, 0, 0, 0, 0, 0, -mRkd;

		mOmegaFluxMat <<
			0, 1, 0, 0, 0, 0, 0,
			-1, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0;
	}
	else
	{
		mInductanceMat = Matrix::Zero(6, 6);
		mResistanceMat = Matrix::Zero(6, 6);
		mReactanceMat = Matrix::Zero(6, 6);
		mOmegaFluxMat = Matrix::Zero(6, 6);

		mInductanceMat <<
			-(mLl + mLmq), 0, 0, mLmq, 0, 0,
			0, -(mLl + mLmd), 0, 0, mLmd, mLmd,
			0, 0, -mLl, 0, 0, 0,
			-mLmq, 0, 0, mLlkq1 + mLmq, 0, 0,
			0, -mLmd, 0, 0, mLlfd + mLmd, mLmd,
			0, -mLmd, 0, 0, mLmd, mLlkd + mLmd;
		mResistanceMat <<
			mRs, 0, 0, 0, 0, 0,
			0, mRs, 0, 0, 0, 0,
			0, 0, mRs, 0, 0, 0,
			0, 0, 0, -mRkq1, 0, 0,
			0, 0, 0, 0, -mRfd, 0,
			0, 0, 0, 0, 0, -mRkd;

		mOmegaFluxMat <<
			0, 1, 0, 0, 0, 0,
			-1, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0;
	}

	mReactanceMat = mInductanceMat.inverse();

	// steady state per unit initial value
	initStatesInPerUnit(initActivePower, initReactivePower, initTerminalVolt, initVoltAngle, initFieldVoltage, initMechPower);

	mVaRe = dq0ToAbcTransform(mThetaMech, mVd* mBase_v, mVq* mBase_v, mV0* mBase_v)(0);
	mVbRe = dq0ToAbcTransform(mThetaMech, mVd* mBase_v, mVq* mBase_v, mV0* mBase_v)(1);
	mVcRe = dq0ToAbcTransform(mThetaMech, mVd* mBase_v, mVq* mBase_v, mV0* mBase_v)(2);
	mVaIm = dq0ToAbcTransform(mThetaMech, mVd* mBase_v, mVq* mBase_v, mV0* mBase_v)(3);
	mVbIm = dq0ToAbcTransform(mThetaMech, mVd* mBase_v, mVq* mBase_v, mV0* mBase_v)(4);
	mVcIm = dq0ToAbcTransform(mThetaMech, mVd* mBase_v, mVq* mBase_v, mV0* mBase_v)(5);

	mIaRe = dq0ToAbcTransform(mThetaMech, mId * mBase_i, mIq * mBase_i, mI0 * mBase_i)(0);
	mIbRe = dq0ToAbcTransform(mThetaMech, mId * mBase_i, mIq * mBase_i, mI0 * mBase_i)(1);
	mIcRe = dq0ToAbcTransform(mThetaMech, mId * mBase_i, mIq * mBase_i, mI0 * mBase_i)(2);
	mIaIm = dq0ToAbcTransform(mThetaMech, mId * mBase_i, mIq * mBase_i, mI0 * mBase_i)(3);
	mIbIm = dq0ToAbcTransform(mThetaMech, mId * mBase_i, mIq * mBase_i, mI0 * mBase_i)(4);
	mIcIm = dq0ToAbcTransform(mThetaMech, mId * mBase_i, mIq * mBase_i, mI0 * mBase_i)(5);
}

void SynchronGenerator::initStatesInPerUnit(Real initActivePower, Real initReactivePower,
	Real initTerminalVolt, Real initVoltAngle, Real initFieldVoltage, Real initMechPower) {

	// #### Electrical variables ##############################################
	Real init_P = initActivePower / mNomPower;
	Real init_Q = initReactivePower / mNomPower;
	Real init_S = sqrt(pow(init_P, 2.) + pow(init_Q, 2.));
	Real init_vt = initTerminalVolt / mBase_v;
	Real init_it = init_S / init_vt;

	// power factor
	Real init_pf = acos(init_P / init_S);

	// load angle
	Real init_delta = atan(((mLmq + mLl) * init_it * cos(init_pf) - mRs * init_it * sin(init_pf)) /
		(init_vt + mRs * init_it * cos(init_pf) + (mLmq + mLl) * init_it * sin(init_pf)));
	Real init_delta_deg = init_delta / DPS_PI * 180;

	// dq stator voltages and currents
	Real init_vd = init_vt * sin(init_delta);
	Real init_vq = init_vt * cos(init_delta);
	Real init_id = init_it * sin(init_delta + init_pf);
	Real init_iq = init_it * cos(init_delta + init_pf);

	// rotor voltage and current
	Real init_ifd = (init_vq + mRs * init_iq + (mLmd + mLl) * init_id) / mLmd;
	Real init_vfd = mRfd * init_ifd;

	// flux linkages
	Real init_psid = init_vq + mRs * init_iq;
	Real init_psiq = -init_vd - mRs * init_id;
	Real init_psifd = (mLmd + mLlfd) * init_ifd - mLmd * init_id;
	Real init_psid1 = mLmd * (init_ifd - init_id);
	Real init_psiq1 = -mLmq * init_iq;
	Real init_psiq2 = -mLmq * init_iq;

	// rotor mechanical variables
	Real init_Te = init_P + mRs * pow(init_it, 2.);
	mOmMech = 1;

	mVoltages(0, 0) = init_vq;
	mVoltages(1, 0) = init_vd;
	mVoltages(2, 0) = 0;
	mVoltages(3, 0) = 0;
	mVoltages(4, 0) = 0;
	mVoltages(5, 0) = init_vfd;
	mVoltages(6, 0) = 0;

	mVd = init_vd;
	mVq = init_vq;
	mV0 = 0;
	mVfd = init_vfd;
	mVkd = 0;
	mVkq1 = 0;
	mVkq2 = 0;

	mCurrents(0, 0) = init_iq;
	mCurrents(1, 0) = init_id;
	mCurrents(2, 0) = 0;
	mCurrents(3, 0) = 0;
	mCurrents(4, 0) = 0;
	mCurrents(5, 0) = init_ifd;
	mCurrents(6, 0) = 0;

	mId = init_id;
	mIq = init_iq;
	mI0 = 0;
	mIfd = init_ifd;
	mIkd = 0;
	mIkq1 = 0;
	mIkq2 = 0;

	mFluxes(0, 0) = init_psiq;
	mFluxes(1, 0) = init_psid;
	mFluxes(2, 0) = 0;
	mFluxes(3, 0) = init_psiq1;
	mFluxes(4, 0) = init_psiq2;
	mFluxes(5, 0) = init_psifd;
	mFluxes(6, 0) = init_psid1;

	mPsid = init_psid;
	mPsiq = init_psiq;
	mPsi0 = 0;
	mPsifd = init_psifd;
	mPsikd = init_psid1;
	mPsikq1 = init_psiq1;
	mPsikq2 = init_psiq2;

	// #### mechanical variables ##############################################
	mMechPower = initMechPower / mNomPower;
	mMechTorque = mMechPower / 1;
	mThetaMech = initVoltAngle + init_delta - PI / 2.;
}

void SynchronGenerator::step(SystemModel& system, Real time) {

	stepInPerUnit(system.getOmega(), system.getTimeStep(), time, system.getNumMethod());

	if (mNode1 >= 0) {
		system.addCompToRightSideVector(mNode1, mIaRe, mIaIm);
	}
	if (mNode2 >= 0) {
		system.addCompToRightSideVector(mNode2, mIbRe, mIbIm);
	}
	if (mNode3 >= 0) {
		system.addCompToRightSideVector(mNode3, mIcRe, mIcIm);
	}

	if (mLogActive) {
		Matrix logValues(getFluxes().rows() + getVoltages().rows() + getCurrents().rows() + 3, 1);
		logValues << getFluxes(), getVoltages(), getCurrents(), getElectricalTorque(), getRotationalSpeed(), getRotorPosition();
		mLog->LogDataLine(time, logValues);
	}
}

void SynchronGenerator::stepInPerUnit(Real om, Real dt, Real time, NumericalMethod numMethod) {

	mVaRe = (1 / mBase_v) * mVaRe;
	mVaIm = (1 / mBase_v) * mVaIm;
	mVbRe = (1 / mBase_v) * mVbRe;
	mVbIm = (1 / mBase_v) * mVbIm;
	mVcRe = (1 / mBase_v) * mVcRe;
	mVcIm = (1 / mBase_v) * mVcIm;

	mIaRe = (1 / mBase_i) * mIaRe;
	mIaIm = (1 / mBase_i) * mIaIm;
	mIbRe = (1 / mBase_i) * mIbRe;
	mIbIm = (1 / mBase_i) * mIbIm;
	mIcRe = (1 / mBase_i) * mIcRe;
	mIcIm = (1 / mBase_i) * mIcIm;

	mVq = abcToDq0Transform(mThetaMech, mVaRe, mVbRe, mVcRe, mVaIm, mVbIm, mVcIm)(0);
	mVd = abcToDq0Transform(mThetaMech, mVaRe, mVbRe, mVcRe, mVaIm, mVbIm, mVcIm)(1);
	mV0 = abcToDq0Transform(mThetaMech, mVaRe, mVbRe, mVcRe, mVaIm, mVbIm, mVcIm)(2);

	if (numMethod == NumericalMethod::Trapezoidal_current) {

		if (DampingWindings == 2)
		{
			Matrix A = (mReactanceMat*mResistanceMat);
			Matrix B = mReactanceMat;
			Matrix C = Matrix::Zero(7, 1);
			C(0, 0) = -mOmMech*mPsid;
			C(1, 0) = mOmMech*mPsiq;
			C = mReactanceMat*C;

			Matrix dqCurrents(7, 1);
			dqCurrents(0, 0) = mIq;
			dqCurrents(1, 0) = mId;
			dqCurrents(2, 0) = mI0;
			dqCurrents(3, 0) = mIkq1;
			dqCurrents(4, 0) = mIkq2;
			dqCurrents(5, 0) = mIfd;
			dqCurrents(6, 0) = mIkd;

			Matrix dqVoltages(7, 1);
			dqVoltages(0, 0) = mVq;
			dqVoltages(1, 0) = mVd;
			dqVoltages(2, 0) = mV0;
			dqVoltages(3, 0) = mVkq1;
			dqVoltages(4, 0) = mVkq2;
			dqVoltages(5, 0) = mVfd;
			dqVoltages(6, 0) = mVkd;
			dqCurrents = Trapezoidal(dqCurrents, A, B, C, dt*mOmMech, dqVoltages);

			mIq = dqCurrents(0, 0);
			mId = dqCurrents(1, 0);
			mI0 = dqCurrents(2, 0);
			mIkq1 = dqCurrents(3, 0);
			mIkq2 = dqCurrents(4, 0);
			mIfd = dqCurrents(5, 0);
			mIkd = dqCurrents(6, 0);

			//Calculation of currents based on inverse of inductance matrix
			mPsiq = -(mLl + mLmq)*mIq + mLmq*mIkq1 + mLmq*mIkq2;
			mPsid = -(mLl + mLmd)*mId + mLmd*mIfd + mLmd*mIkd;
			mPsi0 = -mLl*mI0;
			mPsikq1 = -mLmq*mIq + (mLlkq1 + mLmq)*mIkq1 + mLmq*mIkq2;
			mPsikq2 = -mLmq*mIq + mLmq*mIkq1 + (mLlkq2 + mLmq)*mIkq2;
			mPsifd = -mLmd*mId + (mLlfd + mLmd)*mIfd + mLmd*mIkd;
			mPsikd = -mLmd*mId + mLmd*mIfd + (mLlkd + mLmd)*mIkd;
		}

		else
		{
			Matrix A = (mReactanceMat*mResistanceMat);
			Matrix B = mReactanceMat;
			Matrix C = Matrix::Zero(6, 1);
			C(0, 0) = -mOmMech*mPsid;
			C(1, 0) = mOmMech*mPsiq;
			C = mReactanceMat*C;

			Matrix dqCurrents(6, 1);
			dqCurrents(0, 0) = mIq;
			dqCurrents(1, 0) = mId;
			dqCurrents(2, 0) = mI0;
			dqCurrents(3, 0) = mIkq1;
			dqCurrents(4, 0) = mIfd;
			dqCurrents(5, 0) = mIkd;

			Matrix dqVoltages(6, 1);
			dqVoltages(0, 0) = mVq;
			dqVoltages(1, 0) = mVd;
			dqVoltages(2, 0) = mV0;
			dqVoltages(3, 0) = mVkq1;
			dqVoltages(4, 0) = mVfd;
			dqVoltages(5, 0) = mVkd;

			dqCurrents = Trapezoidal(dqCurrents, A, B, C, dt*mBase_OmElec, dqVoltages);

			mIq = dqCurrents(0, 0);
			mId = dqCurrents(1, 0);
			mI0 = dqCurrents(2, 0);
			mIkq1 = dqCurrents(3, 0);
			mIfd = dqCurrents(4, 0);
			mIkd = dqCurrents(5, 0);

			//Calculation of currents based on inverse of inductance matrix
			mPsiq = -(mLl + mLmq)*mIq + mLmq*mIkq1;
			mPsid = -(mLl + mLmd)*mId + mLmd*mIfd + mLmd*mIkd;
			mPsi0 = -mLl*mI0;
			mPsikq1 = -mLmq*mIq + (mLlkq1 + mLmq)*mIkq1;
			mPsifd = -mLmd*mId + (mLlfd + mLmd)*mIfd + mLmd*mIkd;
			mPsikd = -mLmd*mId + mLmd*mIfd + (mLlkd + mLmd)*mIkd;
		}

	}

	else {

		// Calculation of rotational speed with euler
		mElecTorque = (mPsid*mIq - mPsiq*mId);
		mOmMech = mOmMech + dt * (1 / (2 * mH) * (mMechTorque - mElecTorque));

		//Calculation of flux
		if (DampingWindings == 2)
		{
			Matrix A = (mResistanceMat*mReactanceMat - mOmMech*mOmegaFluxMat);
			Matrix B = Matrix::Identity(7, 7);

			Matrix Fluxes(7, 1);
			Fluxes(0, 0) = mPsiq;
			Fluxes(1, 0) = mPsid;
			Fluxes(2, 0) = mPsi0;
			Fluxes(3, 0) = mPsikq1;
			Fluxes(4, 0) = mPsikq2;
			Fluxes(5, 0) = mPsifd;
			Fluxes(6, 0) = mPsikd;

			Matrix dqVoltages(7, 1);
			dqVoltages(0, 0) = mVq;
			dqVoltages(1, 0) = mVd;
			dqVoltages(2, 0) = mV0;
			dqVoltages(3, 0) = mVkq1;
			dqVoltages(4, 0) = mVkq2;
			dqVoltages(5, 0) = mVfd;
			dqVoltages(6, 0) = mVkd;

			if (numMethod == NumericalMethod::Trapezoidal_flux)
				Fluxes = Trapezoidal(Fluxes, A, B, dt*mBase_OmElec, dqVoltages);
			else
				Fluxes = Euler(Fluxes, A, B, dt*mBase_OmElec, dqVoltages);

			mPsiq = Fluxes(0, 0);
			mPsid = Fluxes(1, 0);
			mPsi0 = Fluxes(2, 0);
			mPsikq1 = Fluxes(3, 0);
			mPsikq2 = Fluxes(4, 0);
			mPsifd = Fluxes(5, 0);
			mPsikd = Fluxes(6, 0);

		}
		else
		{
			Matrix A = (mResistanceMat*mReactanceMat - mOmMech*mOmegaFluxMat);
			Matrix B = Matrix::Identity(6, 6);

			Matrix Fluxes(6, 1);
			Fluxes(0, 0) = mPsiq;
			Fluxes(1, 0) = mPsid;
			Fluxes(2, 0) = mPsi0;
			Fluxes(3, 0) = mPsikq1;
			Fluxes(4, 0) = mPsifd;
			Fluxes(5, 0) = mPsikd;

			Matrix dqVoltages(6, 1);
			dqVoltages(0, 0) = mVq;
			dqVoltages(1, 0) = mVd;
			dqVoltages(2, 0) = mV0;
			dqVoltages(3, 0) = mVkq1;
			dqVoltages(4, 0) = mVfd;
			dqVoltages(5, 0) = mVkd;

			if (numMethod == NumericalMethod::Trapezoidal_flux)
				Fluxes = Trapezoidal(Fluxes, A, B, dt*mBase_OmElec, dqVoltages);
			else
				Fluxes = Euler(Fluxes, A, B, dt*mBase_OmElec, dqVoltages);

			mPsiq = Fluxes(0, 0);
			mPsid = Fluxes(1, 0);
			mPsi0 = Fluxes(2, 0);
			mPsikq1 = Fluxes(3, 0);
			mPsifd = Fluxes(4, 0);
			mPsikd = Fluxes(5, 0);
		}


		// Calculation of currents based on inverse of inductance matrix
		mId = ((mLlfd*mLlkd + mLmd*(mLlfd + mLlkd))*mPsid - mLmd*mLlkd*mPsifd - mLlfd*mLmd*mPsikd) / detLd;
		mIfd = (mLlkd*mLmd*mPsid - (mLl*mLlkd + mLmd*(mLl + mLlkd))*mPsifd + mLmd*mLl*mPsikd) / detLd;
		mIkd = (mLmd*mLlfd*mPsid + mLmd*mLl*mPsifd - (mLmd*(mLlfd + mLl) + mLl*mLlfd)*mPsikd) / detLd;
		if (DampingWindings == 2)
		{
			mIq = ((mLlkq1*mLlkq2 + mLmq*(mLlkq1 + mLlkq2))*mPsiq - mLmq*mLlkq2*mPsikq1 - mLmq*mLlkq1*mPsikq2) / detLq;
			mIkq1 = (mLmq*mLlkq2*mPsiq - (mLmq*(mLlkq2 + mLl) + mLl*mLlkq2)*mPsikq1 + mLmq*mLl*mPsikq2) / detLq;
			mIkq2 = (mLmq*mLlkq1*mPsiq + mLmq*mLl*mPsikq1 - (mLmq*(mLlkq1 + mLl) + mLl*mLlkq1)*mPsikq2) / detLq;
		}
		else
		{
			mIq = ((mLlkq1 + mLmq)*mPsiq - mLmq*mPsikq1) / detLq;
			mIkq1 = (mLmq*mPsiq - (mLl + mLmq)*mPsikq1) / detLq;
		}
		mI0 = -mPsi0 / mLl;
	}
	// Update mechanical rotor angle with respect to electrical angle
	mThetaMech = mThetaMech + dt * ((mOmMech - 1) * mBase_OmMech);

	mIaRe = mBase_i * dq0ToAbcTransform(mThetaMech, mId, mIq, mI0)(0);
	mIbRe = mBase_i * dq0ToAbcTransform(mThetaMech, mId, mIq, mI0)(1);
	mIcRe = mBase_i * dq0ToAbcTransform(mThetaMech, mId, mIq, mI0)(2);
	mIaIm = mBase_i * dq0ToAbcTransform(mThetaMech, mId, mIq, mI0)(3);
	mIbIm = mBase_i * dq0ToAbcTransform(mThetaMech, mId, mIq, mI0)(4);
	mIcIm = mBase_i * dq0ToAbcTransform(mThetaMech, mId, mIq, mI0)(5);

	if (DampingWindings == 2)
	{
		mCurrents2 << mIq,
			mId,
			mI0,
			mIkq1,
			mIkq2,
			mIfd,
			mIkd;

		mVoltages2 << mVq,
			mVd,
			mV0,
			mVkq1,
			mVkq2,
			mVfd,
			mVkd;

		mFluxes2 << mPsiq,
			mPsid,
			mPsi0,
			mPsikq1,
			mPsikq2,
			mPsifd,
			mPsikd;
	}
	else
	{
		mCurrents2 << mIq,
			mId,
			mI0,
			mIkq1,
			mIfd,
			mIkd;

		mVoltages2 << mVq,
			mVd,
			mV0,
			mVkq1,
			mVfd,
			mVkd;

		mFluxes2 << mPsiq,
			mPsid,
			mPsi0,
			mPsikq1,
			mPsifd,
			mPsikd;
	}

}


void SynchronGenerator::postStep(SystemModel& system) {
	if (mNode1 >= 0) {
		mVaRe = system.getRealFromLeftSideVector(mNode1);
		mVaIm = system.getImagFromLeftSideVector(mNode1);
	}
	else {
		mVaRe = 0;
		mVaIm = 0;
	}
	if (mNode2 >= 0) {
		mVbRe = system.getRealFromLeftSideVector(mNode2);
		mVbIm = system.getImagFromLeftSideVector(mNode2);
	}
	else {
		mVbRe = 0;
		mVbIm = 0;
	}
	if (mNode3 >= 0) {
		mVcRe = system.getRealFromLeftSideVector(mNode3);
		mVcIm = system.getImagFromLeftSideVector(mNode3);
	}
	else {
		mVcRe = 0;
		mVcIm = 0;
	}
}


Matrix SynchronGenerator::abcToDq0Transform(Real theta, Real aRe, Real bRe, Real cRe, Real aIm, Real bIm, Real cIm) {
	// Balanced case
	Complex alpha(cos(2. / 3. * PI), sin(2. / 3. * PI));
	Complex thetaCompInv(cos(-theta), sin(-theta));
	MatrixComp AbcToPnz(3, 3);
	AbcToPnz <<
		1, 1, 1,
		1, alpha, pow(alpha, 2),
		1, pow(alpha, 2), alpha;
	AbcToPnz = (1. / 3.) * AbcToPnz;

	MatrixComp abcVector(3, 1);
	abcVector <<
		Complex(aRe, aIm),
		Complex(bRe, bIm),
		Complex(cRe, cIm);

	MatrixComp pnzVector(3, 1);
	pnzVector = AbcToPnz * abcVector * thetaCompInv;

	Matrix dq0Vector(3, 1);
	dq0Vector <<
		pnzVector(1, 0).imag(),
		pnzVector(1, 0).real(),
		0;

	return dq0Vector;
}

Matrix SynchronGenerator::dq0ToAbcTransform(Real theta, Real d, Real q, Real zero) {
	// Balanced case
	Complex alpha(cos(2. / 3. * PI), sin(2. / 3. * PI));
	Complex thetaComp(cos(theta), sin(theta));
	MatrixComp PnzToAbc(3, 3);
	PnzToAbc <<
		1, 1, 1,
		1, pow(alpha, 2), alpha,
		1, alpha, pow(alpha, 2);

	MatrixComp pnzVector(3, 1);
	pnzVector <<
		0,
		Complex(d, q),
		Complex(0, 0);

	MatrixComp abcCompVector(3, 1);
	abcCompVector = PnzToAbc * pnzVector * thetaComp;

	Matrix abcVector(6, 1);
	abcVector <<
		abcCompVector(0, 0).real(),
		abcCompVector(1, 0).real(),
		abcCompVector(2, 0).real(),
		abcCompVector(0, 0).imag(),
		abcCompVector(1, 0).imag(),
		abcCompVector(2, 0).imag();

	return abcVector;
}