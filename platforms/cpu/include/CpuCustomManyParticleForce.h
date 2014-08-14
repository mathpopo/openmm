
/* Portions copyright (c) 2009-2014 Stanford University and Simbios.
 * Contributors: Peter Eastman
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS, CONTRIBUTORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef OPENMM_CPU_CUSTOM_MANY_PARTICLE_FORCE_H__
#define OPENMM_CPU_CUSTOM_MANY_PARTICLE_FORCE_H__

#include "ReferenceForce.h"
#include "ReferenceBondIxn.h"
#include "CompiledExpressionSet.h"
#include "CpuNeighborList.h"
#include "openmm/CustomManyParticleForce.h"
#include "openmm/internal/ThreadPool.h"
#include "openmm/internal/vectorize.h"
#include "lepton/CompiledExpression.h"
#include "lepton/ParsedExpression.h"
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace OpenMM {

class CpuCustomManyParticleForce {
private:

    class ParticleTermInfo;
    class DistanceTermInfo;
    class AngleTermInfo;
    class DihedralTermInfo;
    class ComputeForceTask;
    class ThreadData;
    int numParticlesPerSet, numPerParticleParameters, numTypes;
    bool useCutoff, usePeriodic;
    RealOpenMM cutoffDistance;
    RealOpenMM periodicBoxSize[3];
    CpuNeighborList* neighborList;
    ThreadPool& threads;
    std::vector<std::set<int> > exclusions;
    std::vector<int> particleTypes;
    std::vector<int> orderIndex;
    std::vector<std::vector<int> > particleOrder;
    std::vector<ThreadData*> threadData;
    // The following variables are used to make information accessible to the individual threads.
    int numParticles;
    float* posq;
    RealVec const* atomCoordinates;
    RealOpenMM** particleParameters;        
    const std::map<std::string, double>* globalParameters;
    std::vector<AlignedArray<float> >* threadForce;
    bool includeForces, includeEnergy;
    void* atomicCounter;

    /**
     * This routine contains the code executed by each thread.
     */
    void threadComputeForce(ThreadPool& threads, int threadIndex);

    void loopOverInteractions(std::vector<int>& availableParticles, std::vector<int>& particleSet, int loopIndex, int startIndex,
                              RealOpenMM** particleParameters, float* forces, ThreadData& data, const fvec4& boxSize, const fvec4& invBoxSize);

    /**---------------------------------------------------------------------------------------

       Calculate custom interaction for one set of particles

       @param particleSet        the indices of the particles
       @param posq               atom coordinates in float format
       @param atomCoordinates    atom coordinates
       @param particleParameters particle parameter values (particleParameters[particleIndex][parameterIndex])
       @param forces             force array (forces added)
       @param totalEnergy        total energy

       --------------------------------------------------------------------------------------- */

    void calculateOneIxn(std::vector<int>& particleSet,
                         RealOpenMM** particleParameters, float* forces, ThreadData& data, const fvec4& boxSize, const fvec4& invBoxSize);

    /**
     * Compute the displacement and squared distance between two points, optionally using
     * periodic boundary conditions.
     */
    void getDeltaR(const fvec4& posI, const fvec4& posJ, fvec4& deltaR, float& r2, const fvec4& boxSize, const fvec4& invBoxSize) const;

    void computeDelta(int atom1, int atom2, RealOpenMM* delta, const OpenMM::RealVec* atomCoordinates) const;

    static RealOpenMM computeAngle(RealOpenMM* vec1, RealOpenMM* vec2, float sign);


public:

    /**---------------------------------------------------------------------------------------

       Constructor

       --------------------------------------------------------------------------------------- */

    CpuCustomManyParticleForce(const OpenMM::CustomManyParticleForce& force, ThreadPool& threads);

    /**---------------------------------------------------------------------------------------

       Destructor

       --------------------------------------------------------------------------------------- */

    ~CpuCustomManyParticleForce();

    /**---------------------------------------------------------------------------------------

       Set the force to use a cutoff.

       @param distance            the cutoff distance

       --------------------------------------------------------------------------------------- */

    void setUseCutoff(RealOpenMM distance);

    /**---------------------------------------------------------------------------------------

       Set the force to use periodic boundary conditions.  This requires that a cutoff has
       already been set, and the smallest side of the periodic box is at least twice the cutoff
       distance.

       @param boxSize             the X, Y, and Z widths of the periodic box

       --------------------------------------------------------------------------------------- */

    void setPeriodic(OpenMM::RealVec& boxSize);

    /**---------------------------------------------------------------------------------------

       Calculate the interaction

       @param posq               atom coordinates in float format
       @param atomCoordinates    atom coordinates
       @param particleParameters particle parameter values (particleParameters[particleIndex][parameterIndex])
       @param globalParameters   the values of global parameters
       @param forces             force array (forces added)
       @param totalEnergy        total energy

       --------------------------------------------------------------------------------------- */

    void calculateIxn(AlignedArray<float>& posq, std::vector<OpenMM::RealVec>& atomCoordinates, RealOpenMM** particleParameters,
                      const std::map<std::string, double>& globalParameters,
                      std::vector<AlignedArray<float> >& threadForce, bool includeForces, bool includeEnergy, double& energy);
};

class CpuCustomManyParticleForce::ParticleTermInfo {
public:
    std::string name;
    int atom, component, variableIndex;
    Lepton::CompiledExpression forceExpression;
    ParticleTermInfo(const std::string& name, int atom, int component, const Lepton::CompiledExpression& forceExpression, ThreadData& data);
};

class CpuCustomManyParticleForce::DistanceTermInfo {
public:
    std::string name;
    int p1, p2, variableIndex;
    Lepton::CompiledExpression forceExpression;
    int delta;
    float deltaSign;
    DistanceTermInfo(const std::string& name, const std::vector<int>& atoms, const Lepton::CompiledExpression& forceExpression, ThreadData& data);
};

class CpuCustomManyParticleForce::AngleTermInfo {
public:
    std::string name;
    int p1, p2, p3, variableIndex;
    Lepton::CompiledExpression forceExpression;
    int delta1, delta2;
    float delta1Sign, delta2Sign;
    AngleTermInfo(const std::string& name, const std::vector<int>& atoms, const Lepton::CompiledExpression& forceExpression, ThreadData& data);
};

class CpuCustomManyParticleForce::DihedralTermInfo {
public:
    std::string name;
    int p1, p2, p3, p4, variableIndex;
    Lepton::CompiledExpression forceExpression;
    int delta1, delta2, delta3;
    mutable RealOpenMM cross1[3];
    mutable RealOpenMM cross2[3];
    DihedralTermInfo(const std::string& name, const std::vector<int>& atoms, const Lepton::CompiledExpression& forceExpression, ThreadData& data);
};

class CpuCustomManyParticleForce::ThreadData {
public:
    CompiledExpressionSet expressionSet;
    Lepton::CompiledExpression energyExpression;
    std::vector<std::vector<int> > particleParamIndices;
    std::vector<std::pair<int, int> > deltaPairs;
    std::vector<ParticleTermInfo> particleTerms;
    std::vector<DistanceTermInfo> distanceTerms;
    std::vector<AngleTermInfo> angleTerms;
    std::vector<DihedralTermInfo> dihedralTerms;
    double energy;
    ThreadData(const CustomManyParticleForce& force, Lepton::ParsedExpression& energyExpr,
            std::map<std::string, std::vector<int> >& distances, std::map<std::string, std::vector<int> >& angles, std::map<std::string, std::vector<int> >& dihedrals);
    /**
     * Request a pair of particles whose distance or displacement vector is needed in the computation.
     */
    void requestDeltaPair(int p1, int p2, int& pairIndex, float& pairSign, bool allowReversed);
};

} // namespace OpenMM

#endif // OPENMM_CPU_CUSTOM_MANY_PARTICLE_FORCE_H__
