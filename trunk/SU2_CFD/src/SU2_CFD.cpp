/*!
 * \file SU2_CFD.cpp
 * \brief Main file of Computational Fluid Dynamics Code (SU2_CFD).
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

#include "../include/SU2_CFD.hpp"

using namespace std;

int main(int argc, char *argv[]) {
	/*--- Variable definitions ---*/
	bool StopCalc = false;
	unsigned long StartTime, StopTime, TimeUsed = 0, ExtIter = 0;
	unsigned short iMesh, iZone, nZone, nDim;
	ofstream ConvHist_file;
	char file_name[200];
	int rank = MASTER_NODE;
	
#ifndef NO_MPI
	/*--- MPI initialization, and buffer setting ---*/
	void *buffer, *old_buffer;
	int size, bufsize;
	bufsize = MAX_MPI_BUFFER;
	buffer = new char[bufsize];
	MPI::Init(argc, argv);
	MPI::Attach_buffer(buffer, bufsize);
	rank = MPI::COMM_WORLD.Get_rank();
	size = MPI::COMM_WORLD.Get_size();
#ifdef TIME
  /*--- Set up a timer for performance comparisons ---*/
  double start, finish, time;
  MPI::COMM_WORLD.Barrier();
  start = MPI::Wtime();
#endif
#endif

	/*--- Pointer to different structures that will be used throughout the entire code ---*/
	COutput *output = NULL;
	CIntegration ***integration_container = NULL;
	CGeometry ***geometry_container = NULL;
	CSolution ****solution_container = NULL;
	CNumerics *****solver_container = NULL;
	CConfig **config_container = NULL;
	CSurfaceMovement **surface_movement = NULL;
	CVolumetricMovement **grid_movement = NULL;
	CFreeFormChunk*** ffd_chunk = NULL;
	
	/*--- Definition of the containers per zones ---*/
	solution_container = new CSolution***[MAX_ZONES];
	integration_container = new CIntegration**[MAX_ZONES];
	solver_container = new CNumerics****[MAX_ZONES];
	config_container = new CConfig*[MAX_ZONES];
	geometry_container = new CGeometry **[MAX_ZONES];
	surface_movement = new CSurfaceMovement *[MAX_ZONES];
	grid_movement = new CVolumetricMovement *[MAX_ZONES];
	ffd_chunk = new CFreeFormChunk**[MAX_ZONES];
	
	/*--- Check the number of zones and dimensions in the mesh file ---*/
	CConfig *config = NULL;
	if (argc == 2) config = new CConfig(argv[1]);
	else { strcpy (file_name, "default.cfg"); config = new CConfig(file_name); }
	nZone = GetnZone(config->GetMesh_FileName(), config->GetMesh_FileFormat(), config);
	nDim = GetnDim(config->GetMesh_FileName(), config->GetMesh_FileFormat());
	
	for (iZone = 0; iZone < nZone; iZone++) {
		
		/*--- Definition of the configuration class per zones ---*/
		if (argc == 2) config_container[iZone] = new CConfig(argv[1], SU2_CFD, iZone, nZone, VERB_HIGH);
		else { strcpy (file_name, "default.cfg"); config_container[iZone] = new CConfig(file_name, SU2_CFD, iZone, nZone, VERB_HIGH); }
		
#ifndef NO_MPI
		/*--- Change the name of the input-output files for a parallel computation ---*/
		config_container[iZone]->SetFileNameDomain(rank+1);
#endif
		
		/*--- Perform the non-dimensionalization for the flow equations ---*/
		config_container[iZone]->SetNondimensionalization(nDim, iZone);
		
		/*--- Definition of the geometry class and open the mesh file ---*/
		geometry_container[iZone] = new CGeometry *[config_container[iZone]->GetMGLevels()+1];
		geometry_container[iZone][MESH_0] = new CPhysicalGeometry(config_container[iZone], config_container[iZone]->GetMesh_FileName(), 
																															config_container[iZone]->GetMesh_FileFormat(), iZone+1, nZone);
		
  }
  
		if (rank == MASTER_NODE)
			cout << endl <<"------------------------- Geometry preprocessing ------------------------" << endl;

		/*--- Definition of the geometry (edge based structure) for all Zones ---*/
		Geometrical_Definition(geometry_container, config_container, nZone);
		  
#ifndef NO_MPI
		/*--- Synchronization point after the geometrical definition subroutine ---*/
		MPI::COMM_WORLD.Barrier();
#endif	
  
		if (rank == MASTER_NODE)
			cout << endl <<"------------------------- Solution preprocessing ------------------------" << endl;

  	for (iZone = 0; iZone < nZone; iZone++) {
      
		/*--- Definition of the solution class (solution_container[#ZONES][#MG_GRIDS][#EQ_SYSTEMS]) ---*/
		solution_container[iZone] = new CSolution** [config_container[iZone]->GetMGLevels()+1];
		for (iMesh = 0; iMesh <= config_container[iZone]->GetMGLevels(); iMesh++)
			solution_container[iZone][iMesh] = new CSolution* [MAX_SOLS];
		Solution_Definition(solution_container[iZone], geometry_container[iZone], config_container[iZone], iZone);
		
#ifndef NO_MPI
		/*--- Synchronization point after the solution definition subroutine ---*/
		MPI::COMM_WORLD.Barrier();
#endif	
		
		if (rank == MASTER_NODE)
			cout << endl <<"------------------ Integration and solver preprocessing -----------------" << endl;

		/*--- Definition of the integration class (integration_container[#ZONES][#EQ_SYSTEMS]) ---*/
		integration_container[iZone] = new CIntegration*[MAX_SOLS];
		Integration_Definition(integration_container[iZone], geometry_container[iZone], config_container[iZone], iZone);
		
#ifndef NO_MPI
		/*--- Synchronization point after the integration definition subrotuine ---*/
		MPI::COMM_WORLD.Barrier();
#endif	
		
		/*--- Definition of the numerical method class (solver_container[#ZONES][#MG_GRIDS][#EQ_SYSTEMS][#EQ_TERMS]) ---*/
		solver_container[iZone] = new CNumerics***[config_container[iZone]->GetMGLevels()+1];
		Solver_Definition(solver_container[iZone], solution_container[iZone], geometry_container[iZone], config_container[iZone], iZone);

#ifndef NO_MPI
		/*--- Synchronization point after the solver definition subrotuine ---*/
		MPI::COMM_WORLD.Barrier();
#endif	
		
		/*--- Computation of wall distance ---*/
		if ((config_container[iZone]->GetKind_Solver() == RANS) || (config_container[iZone]->GetKind_Solver() == ADJ_RANS))
			geometry_container[iZone][MESH_0]->SetWall_Distance(config_container[iZone]);

		/*--- Computation of positive area in the z-plane  ---*/
		geometry_container[iZone][MESH_0]->SetPositive_ZArea(config_container[iZone]);

		/*--- Set the near-field and interface boundary conditions  ---*/
		for (iMesh = 0; iMesh <= config_container[iZone]->GetMGLevels(); iMesh++) {
			geometry_container[iZone][iMesh]->MatchNearField(config_container[iZone]);
			geometry_container[iZone][iMesh]->MatchInterface(config_container[iZone]); 
		}

		/*--- Instantiate the geometry movement classes for dynamic meshes ---*/
		if (config_container[iZone]->GetGrid_Movement()) {
      if (rank == MASTER_NODE)
        cout << "Set dynamic mesh structure." << endl;
			grid_movement[iZone] = new CVolumetricMovement(geometry_container[iZone][MESH_0]);
			ffd_chunk[iZone] = new CFreeFormChunk*[MAX_NUMBER_CHUNK];
      surface_movement[iZone] = new CSurfaceMovement();
      surface_movement[iZone]->CopyBoundary(geometry_container[iZone][MESH_0], config_container[iZone]);
      if (config_container[iZone]->GetUnsteady_Simulation() == TIME_SPECTRAL)
        SetGrid_Movement(geometry_container[iZone], surface_movement[iZone],
                         grid_movement[iZone], ffd_chunk[iZone], solution_container[iZone], config_container[iZone], iZone, 0);
		}
	}

  	/*--- Set the mesh velocities for the time-spectral case ---*/
  	if (config_container[ZONE_0]->GetUnsteady_Simulation() == TIME_SPECTRAL)
  		SetTimeSpectral_Velocities(geometry_container, config_container, nZone);
  
  /*--- Coupling between zones (only two zones... it can be done in a general way) ---*/
	if (nZone == 2) {
		
		if (rank == MASTER_NODE) 
			cout << endl <<"--------------------- Setting coupling between zones --------------------" << endl;
		
		geometry_container[ZONE_0][MESH_0]->MatchZone(config_container[ZONE_0], geometry_container[ZONE_1][MESH_0], config_container[ZONE_1], ZONE_0, nZone);
		geometry_container[ZONE_1][MESH_0]->MatchZone(config_container[ZONE_1], geometry_container[ZONE_0][MESH_0], config_container[ZONE_0], ZONE_1, nZone);
	}
	
	/*--- Definition of the output class (one for all the zones) ---*/
	output = new COutput();
	
	/*--- Open the convergence history file ---*/
	if (rank == MASTER_NODE)
		output->SetHistory_Header(&ConvHist_file, config_container[ZONE_0]);	
	
	/*--- External loop of the solver ---*/
	if (rank == MASTER_NODE) 
		cout << endl <<"------------------------------ Begin solver -----------------------------" << endl;
	
	while (ExtIter < config_container[ZONE_0]->GetnExtIter()) {
		
		StartTime = clock();
		
		for (iZone = 0; iZone < nZone; iZone++) {
			config_container[iZone]->SetExtIter(ExtIter);
			config_container[iZone]->UpdateCFL(ExtIter);
		}
		
		switch (config_container[ZONE_0]->GetKind_Solver()) {
								
			case EULER: case NAVIER_STOKES: case RANS:				
				MeanFlowIteration(output, integration_container, geometry_container, 
													solution_container, solver_container, config_container, 
													surface_movement, grid_movement, ffd_chunk, ExtIter);
				break;
				
			case PLASMA_EULER: case PLASMA_NAVIER_STOKES:
				PlasmaIteration(output, integration_container, geometry_container,
												solution_container, solver_container, config_container,
												surface_movement, grid_movement, ffd_chunk, ExtIter);
				break;
				
			case FREE_SURFACE_EULER: case FREE_SURFACE_NAVIER_STOKES: case FREE_SURFACE_RANS:
				FreeSurfaceIteration(output, integration_container, geometry_container, 
														 solution_container, solver_container, config_container, 
														 surface_movement, grid_movement, ffd_chunk, ExtIter);
				break;
				
			case FLUID_STRUCTURE_EULER: case FLUID_STRUCTURE_NAVIER_STOKES:
				FluidStructureIteration(output, integration_container, geometry_container, 
																solution_container, solver_container, config_container, 
																surface_movement, grid_movement, ffd_chunk, ExtIter);
				break;
				
			case AEROACOUSTIC_EULER: case AEROACOUSTIC_NAVIER_STOKES:
				AeroacousticIteration(output, integration_container, geometry_container, 
															solution_container, solver_container, config_container, 
															surface_movement, grid_movement, ffd_chunk, ExtIter);
				break;
				
			case WAVE_EQUATION:
				WaveIteration(output, integration_container, geometry_container, 
											solution_container, solver_container, config_container, 
											surface_movement, grid_movement, ffd_chunk, ExtIter);
				break;
				
			case LINEAR_ELASTICITY:
				FEAIteration(output, integration_container, geometry_container, 
										 solution_container, solver_container, config_container, 
										 surface_movement, grid_movement, ffd_chunk, ExtIter);
				break;
				
				
			case ADJ_EULER: case ADJ_NAVIER_STOKES: case ADJ_RANS:
				AdjMeanFlowIteration(output, integration_container, geometry_container, 
														 solution_container, solver_container, config_container, 
														 surface_movement, grid_movement, ffd_chunk, ExtIter);				
				break;
				
			case ADJ_PLASMA_EULER: case ADJ_PLASMA_NAVIER_STOKES:
				AdjPlasmaIteration(output, integration_container, geometry_container, 
													 solution_container, solver_container, config_container, 
													 surface_movement, grid_movement, ffd_chunk, ExtIter);
				break;
				
			case ADJ_FREE_SURFACE_EULER: case ADJ_FREE_SURFACE_NAVIER_STOKES: case ADJ_FREE_SURFACE_RANS:
				AdjFreeSurfaceIteration(output, integration_container, geometry_container, 
																solution_container, solver_container, config_container, 
																surface_movement, grid_movement, ffd_chunk, ExtIter);
				break;

			case ADJ_AEROACOUSTIC_EULER:
				AdjAeroacousticIteration(output, integration_container, geometry_container, 
																 solution_container, solver_container, config_container, 
																 surface_movement, grid_movement, ffd_chunk, ExtIter);
				break;
		}
		


#ifndef NO_MPI
		MPI::COMM_WORLD.Barrier();
#endif
		
		StopTime = clock(); TimeUsed += (StopTime - StartTime);

		/*--- Convergence history for serial and parallel computation ---*/
			if ((ExtIter % config_container[ZONE_0]->GetWrt_Con_Freq() == 0)
					|| ((config_container[ZONE_0]->IsAdjoint()) && (config_container[ZONE_0]->GetKind_Adjoint() == DISCRETE))){
			
				/*--- Evaluate and plot the equivalent area, and flow rate ---*/
				if (config_container[ZONE_0]->GetKind_Solver() == EULER) {
					if (config_container[ZONE_0]->GetEquivArea() == YES)
						output->SetEquivalentArea(solution_container[ZONE_0][MESH_0][FLOW_SOL], geometry_container[ZONE_0][MESH_0], config_container[ZONE_0], ExtIter);
					if (config_container[ZONE_0]->GetFlowRate() == YES)
						output->SetFlowRate(solution_container[ZONE_0][MESH_0][FLOW_SOL], geometry_container[ZONE_0][MESH_0], config_container[ZONE_0], ExtIter);
				}

				/*--- Print the history file --*/
				output->SetHistory_MainIter(&ConvHist_file, geometry_container, solution_container, config_container[ZONE_0], integration_container, ExtIter, TimeUsed, ZONE_0);

			}
		
		/*--- Convergence criteria ---*/
		switch (config_container[ZONE_0]->GetKind_Solver()) {
			case EULER: case NAVIER_STOKES: case RANS:
				StopCalc = integration_container[ZONE_0][FLOW_SOL]->GetConvergence(); break;
			case FREE_SURFACE_EULER: case FREE_SURFACE_NAVIER_STOKES: case FREE_SURFACE_RANS:
				StopCalc = integration_container[ZONE_0][FLOW_SOL]->GetConvergence(); break;
			case PLASMA_EULER: case PLASMA_NAVIER_STOKES:
				StopCalc = integration_container[ZONE_0][PLASMA_SOL]->GetConvergence(); break;
			case WAVE_EQUATION:
				StopCalc = integration_container[ZONE_0][WAVE_SOL]->GetConvergence(); break;
			case LINEAR_ELASTICITY:
				StopCalc = integration_container[ZONE_0][FEA_SOL]->GetConvergence(); break;				
			case ADJ_EULER: case ADJ_NAVIER_STOKES: case ADJ_RANS:
				StopCalc = integration_container[ZONE_0][ADJFLOW_SOL]->GetConvergence(); break;
			case ADJ_FREE_SURFACE_EULER: case ADJ_FREE_SURFACE_NAVIER_STOKES: case ADJ_FREE_SURFACE_RANS:
				StopCalc = integration_container[ZONE_0][ADJFLOW_SOL]->GetConvergence(); break;
			case ADJ_PLASMA_EULER: case ADJ_PLASMA_NAVIER_STOKES:
				StopCalc = integration_container[ZONE_0][ADJPLASMA_SOL]->GetConvergence(); break;
		}
		


		/*--- Solution output ---*/
		if ( (((ExtIter+1 == config_container[ZONE_0]->GetnExtIter()) || 
					 (ExtIter % config_container[ZONE_0]->GetWrt_Sol_Freq() == 0) || StopCalc ) && (ExtIter != 0)) || 
				(config_container[ZONE_0]->GetnExtIter() == 1) || 
				(((config_container[ZONE_0]->GetUnsteady_Simulation() == DT_STEPPING_1ST) || 
					(config_container[ZONE_0]->GetUnsteady_Simulation() == DT_STEPPING_2ND)) && (ExtIter == 0)) ) {
			output->SetResult_Files(solution_container, geometry_container, config_container, ExtIter, nZone);
		}



		/*--- Stop criteria	---*/	
		if (StopCalc) break;
		
		ExtIter++;

	}
	

	if ((config_container[ZONE_0]->IsAdjoint()) && (config_container[ZONE_0]->GetShow_Adj_Sens())) {

		cout << endl;
		cout << "Adjoint-derived sensitivities:" << endl;
		cout << "Surface sensitivity = " << solution_container[ZONE_0][MESH_0][ADJFLOW_SOL]->GetTotal_Sens_Geo() << endl;
		cout << "Mach number sensitivity = " << solution_container[ZONE_0][MESH_0][ADJFLOW_SOL]->GetTotal_Sens_Mach() << endl;
		cout << "Angle of attack sensitivity = " << solution_container[ZONE_0][MESH_0][ADJFLOW_SOL]->GetTotal_Sens_AoA() << endl;
		cout << "Pressure sensitivity = " << solution_container[ZONE_0][MESH_0][ADJFLOW_SOL]->GetTotal_Sens_Press() << endl;
		cout << "Temperature sensitivity = " << solution_container[ZONE_0][MESH_0][ADJFLOW_SOL]->GetTotal_Sens_Temp() << endl;
		cout << endl;

	}

	//if ((!config_container[ZONE_0]->IsAdjoint()) || (config_container[ZONE_0]->GetKind_Adjoint() != DISCRETE))
		ConvHist_file.close();
	
	/*--- Memory deallocation ---*/
	//Solver_Deallocation(solver_container[iZone], solution_container, integration_container[ZONE_0], output, geometry_container, config_container[iZone]);
	//Geometrical_Deallocation(geometry_container, config_container[iZone]);
		
#ifndef NO_MPI
  /*--- Compute and print the total time for scaling tests. ---*/
#ifdef TIME
  MPI::COMM_WORLD.Barrier();
  finish = MPI::Wtime();
  time = finish-start;
  if (rank == MASTER_NODE) {
    if (size == 1) {cout << "\nCompleted in " << time << " seconds on ";
      cout << size << " core.\n" << endl;
    } else {
      cout << "\nCompleted in " << time << " seconds on " << size;
      cout << " cores.\n" << endl;
    }
  }
#endif
	/*--- Finalize MPI parallelization ---*/
	old_buffer = buffer;
	MPI::Detach_buffer(old_buffer);
	//	delete [] buffer;
	MPI::Finalize();
#endif
	
	/*--- End solver ---*/
	if (rank == MASTER_NODE) 
	  cout << endl <<"------------------------- Exit Success (SU2_CFD) ------------------------" << endl << endl;
	
	return EXIT_SUCCESS;
}
