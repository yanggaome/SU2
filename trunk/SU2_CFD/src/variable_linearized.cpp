/*!
 * \file variable_linearized.cpp
 * \brief Definition of the solution fields.
 * \author Aerospace Design Laboratory (Stanford University) <http://su2.stanford.edu>.
 * \version 2.0.1
 *
 * Stanford University Unstructured (SU2) Code
 * Copyright (C) 2012 Aerospace Design Laboratory
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/variable_structure.hpp"

CLinEulerVariable::CLinEulerVariable(void) : CVariable() {}

CLinEulerVariable::~CLinEulerVariable(void) {
	delete [] Res_Conv;
	delete [] Res_Visc;
	delete [] Res_Sour;
	delete [] Residual_Sum;
	delete [] Residual_Old;
	delete [] Undivided_Laplacian;
	delete [] Limiter;
	delete [] Grad_AuxVar;
	delete [] Solution_time_n;
	delete [] Solution_time_n1;
	delete [] Res_TruncError;
	
	for (unsigned short iVar = 0; iVar < nVar; iVar++)
		delete [] Res_Visc_RK[iVar];
	delete [] Res_Visc_RK;
}

CLinEulerVariable::CLinEulerVariable(double *val_solution, unsigned short val_ndim, unsigned short val_nvar, CConfig *config) 
: CVariable(val_ndim, val_nvar, config) {
	
	// Allocate structures
	Res_Conv = new double [nVar];
	Res_Visc = new double [nVar];
	Res_Sour = new double [nVar];
	Residual_Sum = new double [nVar];
	Residual_Old = new double [nVar];
	Res_Visc_RK = new double* [nVar];
	for (unsigned short iVar = 0; iVar < nVar; iVar++)
		Res_Visc_RK[iVar] = new double [nVar];
	
	Undivided_Laplacian = new double [nVar];
	Limiter = new double [nVar];
	
	Grad_AuxVar = new double [nDim];
	Solution_time_n = new double [nVar];
	Solution_time_n1 = new double [nVar];
	
	Res_TruncError = new double [nVar];
	for (unsigned short iVar = 0; iVar < nVar; iVar++)
		Res_TruncError[iVar] = 0.0;
	
	// Initialization of variables
	ForceProj_Vector = new double [nDim];
	for (unsigned short iDim = 0; iDim < nDim; iDim++)
		ForceProj_Vector[iDim] = 0.0;
	
	for (unsigned short iVar = 0; iVar < nDim+2; iVar++) {
		Solution[iVar] = val_solution[iVar];
		Solution_Old[iVar] = val_solution[iVar];
	}
}

CLinEulerVariable::CLinEulerVariable(double val_deltarho, double *val_deltavel,
					 double val_deltae, unsigned short val_ndim, unsigned short val_nvar, CConfig *config) 
: CVariable(val_ndim, val_nvar, config) {
	
	// Allocate structures
	Res_Conv = new double [nVar];
	Res_Visc = new double [nVar];
	Res_Sour = new double [nVar];
	Residual_Sum = new double [nVar];
	Residual_Old = new double [nVar];
	Res_Visc_RK = new double* [nVar];
	for (unsigned short iVar = 0; iVar < nVar; iVar++)
		Res_Visc_RK[iVar] = new double [nVar];
	
	Undivided_Laplacian = new double [nVar];
	Limiter = new double [nVar];
	
	Grad_AuxVar = new double [nDim];
	Solution_time_n = new double [nVar];
	Solution_time_n1 = new double [nVar];
	
	Res_TruncError = new double [nVar];
	for (unsigned short iVar = 0; iVar < nVar; iVar++)
		Res_TruncError[iVar] = 0.0;
	
	// Initialization of variables
	ForceProj_Vector = new double [nDim];
	Solution[0] = val_deltarho; 	Solution_Old[0] = val_deltarho;
	Solution[nVar-1] = val_deltae; Solution_Old[nVar-1] = val_deltae;
	
	for (unsigned short iDim = 0; iDim < nDim; iDim++) {
		Solution[iDim+1] = val_deltavel[iDim];
		Solution_Old[iDim+1] = val_deltavel[iDim];
	}
}

void CLinEulerVariable::SetDeltaPressure(double *val_velocity, double Gamma) {
	
	double Mod_Vel = 0.0;
	double Vel_dot_DeltaRhoVel = 0.0;
	for (unsigned short iDim = 0; iDim < nDim; iDim++) {
		Mod_Vel += val_velocity[iDim] * val_velocity[iDim];
		Vel_dot_DeltaRhoVel += val_velocity[iDim] * Solution[iDim+1];
	}
	
	DeltaPressure = 0.5*Solution[0]*Mod_Vel+(Gamma-1.0)*(Solution[nVar-1]-Vel_dot_DeltaRhoVel);
}
