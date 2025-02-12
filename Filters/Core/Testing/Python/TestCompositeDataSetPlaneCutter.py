from vtkmodules.vtkCommonDataModel import vtkPlane
from vtkmodules.vtkFiltersCore import (
    vtkConvertToMultiBlockDataSet,
    vtkPlaneCutter,
)
from vtkmodules.vtkIOIOSS import vtkIOSSReader
from vtkmodules.vtkRenderingCore import (
    vtkActor,
    vtkRenderWindow,
    vtkRenderWindowInteractor,
    vtkRenderer,
)
from vtkmodules.vtkRenderingOpenGL2 import vtkCompositePolyDataMapper2
import vtkmodules.vtkInteractionStyle
import vtkmodules.vtkRenderingFreeType
import vtkmodules.vtkRenderingOpenGL2
from vtkmodules.util.misc import vtkGetDataRoot

VTK_DATA_ROOT = vtkGetDataRoot()

res = 50

# Create the RenderWindow, Renderers and both Actors
ren0 = vtkRenderer()
ren1 = vtkRenderer()
renWin = vtkRenderWindow()
renWin.SetMultiSamples(0)
renWin.AddRenderer(ren0)
renWin.AddRenderer(ren1)
iren = vtkRenderWindowInteractor()
iren.SetRenderWindow(renWin)

inputPDCReader = vtkIOSSReader()
inputPDCReader.SetFileName(VTK_DATA_ROOT + "/Data/can.ex2")
inputPDCReader.Update()

inputMBDConverter = vtkConvertToMultiBlockDataSet()
inputMBDConverter.SetInputConnection(inputPDCReader.GetOutputPort())

# The cut plane
plane = vtkPlane()
plane.SetOrigin(0, 0, 0)
plane.SetNormal(1, 1, 1)

# Accelerated cutter 0
cut0 = vtkPlaneCutter()
cut0.SetInputConnection(inputMBDConverter.GetOutputPort())
cut0.SetPlane(plane)
cut0.ComputeNormalsOff()

sCutterMapper0 = vtkCompositePolyDataMapper2()
sCutterMapper0.SetInputConnection(cut0.GetOutputPort())
sCutterMapper0.ScalarVisibilityOff()

sCutterActor0 = vtkActor()
sCutterActor0.SetMapper(sCutterMapper0)
sCutterActor0.GetProperty().SetColor(1, 1, 1)

# Accelerated cutter 1
cut1 = vtkPlaneCutter()
cut1.SetInputConnection(inputPDCReader.GetOutputPort())
cut1.SetPlane(plane)
cut1.ComputeNormalsOff()

sCutterMapper1 = vtkCompositePolyDataMapper2()
sCutterMapper1.SetInputConnection(cut1.GetOutputPort())
sCutterMapper1.ScalarVisibilityOff()

sCutterActor1 = vtkActor()
sCutterActor1.SetMapper(sCutterMapper1)
sCutterActor1.GetProperty().SetColor(1, 1, 1)

# Add the actors to the renderer, set the background and size
ren0.AddActor(sCutterActor0)
ren1.AddActor(sCutterActor1)
ren0.SetBackground(0, 0, 0)
ren1.SetBackground(0, 0, 0)
ren0.SetViewport(0, 0, 0.5, 1)
ren1.SetViewport(0.5, 0, 1, 1)
renWin.SetSize(600, 300)
ren0.ResetCamera()
ren1.ResetCamera()
iren.Initialize()

renWin.Render()
iren.Start()
