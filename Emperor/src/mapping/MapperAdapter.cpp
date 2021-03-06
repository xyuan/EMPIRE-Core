/*  Copyright &copy; 2013, TU Muenchen, Chair of Structural Analysis,
 *  Stefan Sicklinger, Tianyang Wang, Munich
 *
 *  All rights reserved.
 *
 *  This file is part of EMPIRE.
 *
 *  EMPIRE is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  EMPIRE is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with EMPIRE.  If not, see http://www.gnu.org/licenses/.
 */
#include "MapperAdapter.h"
#include "MortarMapper.h"
#include "IGAMortarMapper.h"
#include "NearestNeighborMapper.h"
#include "BarycentricInterpolationMapper.h"
#include "NearestElementMapper.h"
#include "AbstractMesh.h"
#include "FEMesh.h"
#include "IGAMesh.h"
#include "IGAPatchSurface.h" 	
#include "DataField.h"
#include <iostream>
#include <assert.h>
#include <stdlib.h>
#include "AuxiliaryParameters.h"
#include "AbstractMapper.h"

using namespace std;

namespace EMPIRE {

MapperAdapter::MapperAdapter(std::string _name, AbstractMesh *_meshA, AbstractMesh *_meshB) :
        name(_name), meshA(_meshA), meshB(_meshB) {
    mapperImpl = NULL;
}

MapperAdapter::~MapperAdapter() {
    delete mapperImpl;
}

void MapperAdapter::initMortarMapper(bool oppositeSurfaceNormal, bool dual,
        bool enforceConsistency) {

    MortarMapper::mklSetNumThreads = AuxiliaryParameters::mklSetNumThreads;
    MortarMapper::mapperSetNumThreads = AuxiliaryParameters::mapperSetNumThreads;

    FEMesh *a = NULL;
    FEMesh *b = NULL;

    assert(meshA->type == EMPIRE_Mesh_FEMesh);
    assert(meshB->type == EMPIRE_Mesh_FEMesh);

    FEMesh *feMeshA = dynamic_cast<FEMesh *>(meshA);
    FEMesh *feMeshB = dynamic_cast<FEMesh *>(meshB);

    if (feMeshA->triangulate() == NULL) {
        a = feMeshA;
    } else {
        a = feMeshA->triangulate();
    }

    if (feMeshB->triangulate() == NULL) {
        b = feMeshB;
    } else {
        b = feMeshB->triangulate();
    }

    assert(mapperImpl == NULL);

    mapperImpl = new MortarMapper(a->numNodes, a->numElems, a->numNodesPerElem, a->nodes,
            a->nodeIDs, a->elems, b->numNodes, b->numElems, b->numNodesPerElem, b->nodes,
            b->nodeIDs, b->elems, oppositeSurfaceNormal, dual, enforceConsistency);
}

void MapperAdapter::initIGAMortarMapper(double _tolProjectionDistance, int _numGPsTriangle,
        int _numGPsQuad) {
    bool meshAIGA = (meshA->type == EMPIRE_Mesh_IGAMesh);
    bool meshBIGA = (meshB->type == EMPIRE_Mesh_IGAMesh);
    if (meshAIGA && !meshBIGA) {
        assert(meshB->type == EMPIRE_Mesh_FEMesh);
        mapperImpl = new IGAMortarMapper(name, dynamic_cast<IGAMesh *>(meshA),
                dynamic_cast<FEMesh *>(meshB), _tolProjectionDistance, _numGPsTriangle, _numGPsQuad,
                true);
    } else if (!meshAIGA && meshBIGA) {
        assert(meshA->type == EMPIRE_Mesh_FEMesh);
        mapperImpl = new IGAMortarMapper(name, dynamic_cast<IGAMesh *>(meshB),
                dynamic_cast<FEMesh *>(meshA), _tolProjectionDistance, _numGPsTriangle, _numGPsQuad,
                false);
    } else{
        ERROR_OUT()<<"Error in MapperAdapter::initIGAMortarMapper" << endl;
        ERROR_OUT()<<"Wrong type of mesh!" << endl;
        exit(-1);
    }
}

void MapperAdapter::initNearestNeighborMapper() {
    assert(meshA->type == EMPIRE_Mesh_FEMesh);
    assert(meshB->type == EMPIRE_Mesh_FEMesh);

    FEMesh *a = dynamic_cast<FEMesh *>(meshA);
    FEMesh *b = dynamic_cast<FEMesh *>(meshB);
    assert(mapperImpl == NULL);

    mapperImpl = new NearestNeighborMapper(a->numNodes, a->nodes, b->numNodes, b->nodes);
}

void MapperAdapter::initBarycentricInterpolationMapper() {
    assert(meshA->type == EMPIRE_Mesh_FEMesh);
    assert(meshB->type == EMPIRE_Mesh_FEMesh);

    FEMesh *a = dynamic_cast<FEMesh *>(meshA);
    FEMesh *b = dynamic_cast<FEMesh *>(meshB);
    assert(mapperImpl == NULL);

    mapperImpl = new BarycentricInterpolationMapper(a->numNodes, a->nodes, b->numNodes, b->nodes);
}

void MapperAdapter::initNearestElementMapper() {
    assert(meshA->type == EMPIRE_Mesh_FEMesh);
    assert(meshB->type == EMPIRE_Mesh_FEMesh);

    FEMesh *a = dynamic_cast<FEMesh *>(meshA);
    FEMesh *b = dynamic_cast<FEMesh *>(meshB);
    assert(mapperImpl == NULL);
    NearestElementMapper::mapperSetNumThreads = AuxiliaryParameters::mapperSetNumThreads;

    mapperImpl = new NearestElementMapper(a->numNodes, a->numElems, a->numNodesPerElem, a->nodes,
            a->nodeIDs, a->elems, b->numNodes, b->numElems, b->numNodesPerElem, b->nodes,
            b->nodeIDs, b->elems);
}

void MapperAdapter::consistentMapping(const DataField *fieldA, DataField *fieldB) {

    assert(mapperImpl != NULL);

    int numNodesA, numNodesB;
    if (meshA->type == EMPIRE_Mesh_FEMesh)
        numNodesA = dynamic_cast<FEMesh *>(meshA) -> numNodes;
    else if (meshA->type == EMPIRE_Mesh_IGAMesh)
        numNodesA = dynamic_cast<IGAMesh *>(meshA) ->getNumNodes();
    else{
        ERROR_OUT()<<"Error in MapperAdapter::consistentMapping" << endl;
        ERROR_OUT()<<"Wrong type of meshA!" << endl;
        exit(-1);
    }
    if (meshB->type == EMPIRE_Mesh_FEMesh)
        numNodesB = dynamic_cast<FEMesh *>(meshB) -> numNodes;
    else if (meshB->type == EMPIRE_Mesh_IGAMesh)
        numNodesB = dynamic_cast<IGAMesh *>(meshB) ->getNumNodes();
    else{
        ERROR_OUT()<<"Error in MapperAdapter::consistentMapping" << endl;
        ERROR_OUT()<<"Wrong type of meshB!" << endl;
        exit(-1);
    }

    assert(fieldA->dimension == fieldB->dimension);
    assert(fieldA->numLocations == numNodesA);
    assert(fieldB->numLocations == numNodesB);

    double *fieldADOFi = new double[fieldA->numLocations];
    double *fieldBDOFi = new double[fieldB->numLocations];
    int numDOFs = fieldA->dimension;
    for (int i = 0; i < numDOFs; i++) {
        for (int j = 0; j < fieldA->numLocations; j++)
            fieldADOFi[j] = fieldA->data[j * numDOFs + i];
        mapperImpl->consistentMapping(fieldADOFi, fieldBDOFi);
        for (int j = 0; j < fieldB->numLocations; j++)
            fieldB->data[j * numDOFs + i] = fieldBDOFi[j];
    }
    delete[] fieldADOFi;
    delete[] fieldBDOFi;

}

void MapperAdapter::conservativeMapping(const DataField *fieldB, DataField *fieldA) {
    assert(mapperImpl != NULL);

    int numNodesA, numNodesB;
    if (meshA->type == EMPIRE_Mesh_FEMesh)
        numNodesA = dynamic_cast<FEMesh *>(meshA) -> numNodes;
    else if (meshA->type == EMPIRE_Mesh_IGAMesh)
        numNodesA = dynamic_cast<IGAMesh *>(meshA) ->getNumNodes();
    else{
        ERROR_OUT()<<"Error in MapperAdapter::conservativeMapping" << endl;
        ERROR_OUT()<<"Wrong type of meshA!" << endl;
        exit(-1);
    }
    if (meshB->type == EMPIRE_Mesh_FEMesh)
        numNodesB = dynamic_cast<FEMesh *>(meshB) -> numNodes;
    else if (meshB->type == EMPIRE_Mesh_IGAMesh)
        numNodesB = dynamic_cast<IGAMesh *>(meshB) ->getNumNodes();
    else{
        ERROR_OUT()<<"Error in MapperAdapter::conservativeMapping" << endl;
        ERROR_OUT()<<"Wrong type of meshB!" << endl;
        exit(-1);
    }

    assert(fieldA->dimension == fieldB->dimension);
    assert(fieldA->numLocations == numNodesA);
    assert(fieldB->numLocations == numNodesB);

    double *fieldADOFi = new double[fieldA->numLocations];
    double *fieldBDOFi = new double[fieldB->numLocations];
    int numDOFs = fieldA->dimension;
    for (int i = 0; i < numDOFs; i++) {
        for (int j = 0; j < fieldB->numLocations; j++)
            fieldBDOFi[j] = fieldB->data[j * numDOFs + i];
        mapperImpl->conservativeMapping(fieldBDOFi, fieldADOFi);
        for (int j = 0; j < fieldA->numLocations; j++)
            fieldA->data[j * numDOFs + i] = fieldADOFi[j];
    }
    delete[] fieldADOFi;
    delete[] fieldBDOFi;

}

} /* namespace EMPIRE */
