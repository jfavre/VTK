/*=========================================================================

Program:   ParaView
Module:    vtkCleanUnstructuredGrid.cxx

Copyright (c) Kitware, Inc.
All rights reserved.
See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkCleanUnstructuredGrid.h"

#include "vtkCell.h"
#include "vtkCellData.h"
#include "vtkCollection.h"
#include "vtkDataSet.h"
#include "vtkIncrementalPointLocator.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkIntArray.h"
#include "vtkMergePoints.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPointSet.h"
#include "vtkPoints.h"
#include "vtkRectilinearGrid.h"
#include "vtkUnstructuredGrid.h"

vtkStandardNewMacro(vtkCleanUnstructuredGrid);
vtkCxxSetSmartPointerMacro(vtkCleanUnstructuredGrid, Locator, vtkIncrementalPointLocator);

//----------------------------------------------------------------------------
vtkCleanUnstructuredGrid::vtkCleanUnstructuredGrid() = default;

//----------------------------------------------------------------------------
vtkCleanUnstructuredGrid::~vtkCleanUnstructuredGrid() = default;

//----------------------------------------------------------------------------
void vtkCleanUnstructuredGrid::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  if (this->Locator)
  {
    os << indent << "Locator: ";
    this->Locator->PrintSelf(os, indent.GetNextIndent());
  }
  else
  {
    os << indent << "Locator: none\n";
  }
  os << indent << "ToleranceIsAbsolute: " << (this->ToleranceIsAbsolute ? "On\n" : "Off\n");
  os << indent << "Tolerance: " << this->Tolerance << std::endl;
  os << indent << "AbsoluteTolerance: " << this->AbsoluteTolerance << std::endl;
  os << indent
     << "RemovePointsWithoutCells: " << (this->RemovePointsWithoutCells ? "On\n" : "Off\n");
  os << indent << "OutputPointsPrecision: " << this->OutputPointsPrecision << std::endl;
}

//----------------------------------------------------------------------------
int vtkCleanUnstructuredGrid::RequestData(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkInformation* inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation* outInfo = outputVector->GetInformationObject(0);

  vtkDataSet* input = vtkDataSet::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));
  vtkUnstructuredGrid* output =
    vtkUnstructuredGrid::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));

  if (input->GetNumberOfCells() == 0)
  {
    // set up a ugrid with same data arrays as input, but
    // no points, cells or data.
    output->Allocate(1);
    output->GetPointData()->CopyAllocate(input->GetPointData(), VTK_CELL_SIZE);
    output->GetCellData()->CopyAllocate(input->GetCellData(), 1);
    vtkNew<vtkPoints> pts;
    output->SetPoints(pts);
    return 1;
  }

  output->GetPointData()->CopyAllocate(input->GetPointData());
  output->GetCellData()->PassData(input->GetCellData());

  // First, create a new points array that eliminate duplicate points.
  // Also create a mapping from the old point id to the new.
  vtkNew<vtkPoints> newPts;

  // Set the desired precision for the points in the output.
  if (this->OutputPointsPrecision == vtkAlgorithm::DEFAULT_PRECISION)
  {
    // The logical behaviour would be to use the data type from the input.
    // However, input is a vtkDataSet, which has no point data type; only the
    // derived class vtkPointSet has a vtkPoints attribute, so only for that
    // the logical practice can be applied, while for others (currently
    // vtkImageData and vtkRectilinearGrid) the data type is the default
    // for vtkPoints - which is VTK_FLOAT.
    vtkPointSet* ps = vtkPointSet::SafeDownCast(input);
    if (ps)
    {
      newPts->SetDataType(ps->GetPoints()->GetDataType());
    }
  }
  else if (this->OutputPointsPrecision == vtkAlgorithm::SINGLE_PRECISION)
  {
    newPts->SetDataType(VTK_FLOAT);
  }
  else if (this->OutputPointsPrecision == vtkAlgorithm::DOUBLE_PRECISION)
  {
    newPts->SetDataType(VTK_DOUBLE);
  }

  vtkIdType num = input->GetNumberOfPoints();
  vtkIdType id;
  vtkIdType newId;
  std::vector<vtkIdType> ptMap(num);
  double pt[3];

  this->CreateDefaultLocator(input);
  if (this->ToleranceIsAbsolute)
  {
    this->Locator->SetTolerance(this->AbsoluteTolerance);
  }
  else
  {
    this->Locator->SetTolerance(this->Tolerance * input->GetLength());
  }
  double bounds[6];
  input->GetBounds(bounds);
  this->Locator->InitPointInsertion(newPts, bounds);

  vtkIdType progressStep = num / 100;
  if (progressStep == 0)
  {
    progressStep = 1;
  }

  vtkNew<vtkIdList> pointCells;
  for (id = 0; id < num; ++id)
  {
    if (id % progressStep == 0)
    {
      this->UpdateProgress(0.8 * ((float)id / num));
    }

    bool insert = true;
    if (this->RemovePointsWithoutCells)
    {
      input->GetPointCells(id, pointCells);
      if (pointCells->GetNumberOfIds() == 0)
      {
        insert = false;
      }
    }

    if (insert)
    {
      input->GetPoint(id, pt);
      if (this->Locator->InsertUniquePoint(pt, newId))
      {
        output->GetPointData()->CopyData(input->GetPointData(), id, newId);
      }
      ptMap[id] = newId;
    }
    else
    {
      // Strictly speaking, this is not needed
      // as this is never accessed, but better not let
      // an id undefined.
      ptMap[id] = -1;
    }
  }
  output->SetPoints(newPts);

  // Now copy the cells.
  vtkNew<vtkIdList> cellPoints;
  num = input->GetNumberOfCells();
  output->Allocate(num);
  for (id = 0; id < num; ++id)
  {
    if (id % progressStep == 0)
    {
      this->UpdateProgress(0.8 + 0.2 * (static_cast<float>(id) / num));
    }
    // special handling for polyhedron cells
    if (vtkUnstructuredGrid::SafeDownCast(input) && input->GetCellType(id) == VTK_POLYHEDRON)
    {
      vtkUnstructuredGrid::SafeDownCast(input)->GetFaceStream(id, cellPoints);
      vtkUnstructuredGrid::ConvertFaceStreamPointIds(cellPoints, ptMap.data());
    }
    else
    {
      input->GetCellPoints(id, cellPoints);
      for (int i = 0; i < cellPoints->GetNumberOfIds(); i++)
      {
        int cellPtId = cellPoints->GetId(i);
        newId = ptMap[cellPtId];
        cellPoints->SetId(i, newId);
      }
    }
    output->InsertNextCell(input->GetCellType(id), cellPoints);
  }

  output->Squeeze();
  return 1;
}

//----------------------------------------------------------------------------
int vtkCleanUnstructuredGrid::FillInputPortInformation(int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  return 1;
}

//------------------------------------------------------------------------------
vtkIncrementalPointLocator* vtkCleanUnstructuredGrid::GetLocator()
{
  return this->Locator;
}

//----------------------------------------------------------------------------
void vtkCleanUnstructuredGrid::CreateDefaultLocator(vtkDataSet* input)
{
  double tol;
  if (this->ToleranceIsAbsolute)
  {
    tol = this->AbsoluteTolerance;
  }
  else
  {
    if (input)
    {
      tol = this->Tolerance * input->GetLength();
    }
    else
    {
      tol = this->Tolerance;
    }
  }

  if (this->Locator.Get() == nullptr)
  {
    if (tol == 0.0)
    {
      this->Locator = vtkSmartPointer<vtkMergePoints>::New();
    }
    else
    {
      this->Locator = vtkSmartPointer<vtkPointLocator>::New();
    }
  }
  else
  {
    // check that the tolerance wasn't changed from zero to non-zero
    if ((tol > 0.0) && (this->GetLocator()->GetTolerance() == 0.0))
    {
      this->Locator = vtkSmartPointer<vtkPointLocator>::New();
    }
  }
}
