#include "maya_includes.h"
#include <maya/MTimer.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <queue>
#include <unordered_map>
#include <thread>
#include <functional>
#include "CircularBuffer/src/Comlib.h"

using namespace std;
unordered_map<string, MCallbackId> callbacks;
MObject m_node;
MStatus status = MS::kSuccess;
bool initBool = false;
bool endThread = false;
bool fiveSecondMark = false;

enum NODE_TYPE { TRANSFORM, MESH };
MTimer gTimer;
// keep track of created meshes to maintain them
queue<MObject> newMeshes;
static double totaltime = 0.0;

thread msgThread;

Comlib* producerBuffer;

void nodeAttributeChanged(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* x)
{
	if (msg & MNodeMessage::AttributeMessage::kAttributeEval) {
		MFnMesh mesh(plug.node());
		cout << "Modified: " + mesh.name() << endl;
		MPointArray pointArray;
		mesh.getPoints(pointArray, MSpace::kObject);
		cout << pointArray << endl;
	}
}
void topoChanged(MObject& node, void* data) {
	MFnDependencyNode dn(node);
	auto cbID = MNodeMessage::addAttributeChangedCallback(node, nodeAttributeChanged, NULL, &status);
	if (status == MS::kSuccess) {
		string name = dn.name().asChar();
		string suffix = "AttributeChanged";
		auto itr = callbacks.find(name + suffix);
		if (itr != callbacks.end()) {
			cout << "Callback erased: " + itr->first + "\n";
			MMessage::removeCallback(itr->second);
			callbacks[name + suffix] = cbID;
		}
		else {
			callbacks.insert({ name + suffix, cbID });
		}
	}
	else {
		cout << "Error inserting callback: " + dn.name() + "\n";
	}
}

void nodeTransformChanged(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* userdata) {
	if (msg & MNodeMessage::AttributeMessage::kAttributeSet) {

		MFnTransform tform(plug.node());
		auto transform = tform.transformation();
		cout << "Local transform for " << tform.name() << ": " << transform.asMatrix() << endl << endl;
		MDagPath test1;
		tform.getPath(test1);
		auto worldMtrx = (test1.inclusiveMatrix());
		cout << "Global transform for " << tform.name() << ": " << worldMtrx << endl << endl;
		//Search for children and get their transforms too
		function<void(MObject& node, void* cReturn)> getChildTransforms = [&getChildTransforms](MObject& node, void* cReturn) 
		{
			MFnDagNode dagNode(node);
			MDagPath path;
			for (unsigned int i = 0; i < dagNode.childCount(&status); i++) {
				if (status == MS::kSuccess) {
					dagNode.child(i, &status);
					if (status == MS::kSuccess) {
						if (dagNode.child(i, &status).hasFn(MFn::kTransform)) {
							MFnTransform tformChild(dagNode.child(i, &status));
							tformChild.getPath(path);
							auto worldMtrx = (path.inclusiveMatrix());
							cout << "Global transform for child: " + tformChild.name() + ": " << worldMtrx << endl << endl;
						}
						if (MFnDagNode(dagNode.child(i)).childCount(&status) > 0) {
							if (status == MS::kSuccess) {
								getChildTransforms(dagNode.child(i, &status), cReturn);
							}
						}
					}
				}
			}
		};
		getChildTransforms(plug.node(), nullptr);
	}
}


void nodeNameChanged(MObject& node, const MString& str, void* clientData)
{
	if (node.hasFn(MFn::kDependencyNode)) {
		MFnDependencyNode dn(node);
		if (status == MS::kSuccess) {
			cout << "Node " + str + " has been renamed to " + dn.name() + '\n';
		}
	}
}

void dirtyNode(MObject& node, void* clientData)
{
	//if (node.hasFn(MFn::kTransform)) {
	//	MFnTransform tform(node);
	//	//cout << tform.name() << " suspects their transform is changed" << endl;
	//	auto transform = tform.transformation();
	//	cout << "Local transform for " << tform.name() << ": " << transform.asMatrix() << endl << endl;
	//	MDagPath test1;
	//	tform.getPath(test1);
	//	auto worldMtrx = (test1.inclusiveMatrix());
	//	cout << "dagpath: " << test1.fullPathName() << endl;
	//	cout << "Global transform for " << tform.name() << ": " << worldMtrx << endl << endl;
	//}
}
/*
 * how Maya calls this method when a node is added.
 * new POLY mesh: kPolyXXX, kTransform, kMesh
 * new MATERIAL : kBlinn, kShadingEngine, kMaterialInfo
 * new LIGHT    : kTransform, [kPointLight, kDirLight, kAmbientLight]
 * new JOINT    : kJoint
 */
void nodeAdded(MObject& node, void* clientData) {
	if (node.hasFn(MFn::kDependencyNode)) {
		MFnDependencyNode dn(node);
		cout << "Node added: " << dn.name() + '\n';
		MCallbackId cbID = MNodeMessage::addNameChangedCallback(node, nodeNameChanged, NULL, &status);
		if (status == MS::kSuccess) {
			string name = dn.name().asChar();
			string suffix = "NameChanged";
			auto itr = callbacks.find(name + suffix);
			if (itr != callbacks.end()) {
				cout << "Callback erased: " + itr->first + "\n";
				MMessage::removeCallback(itr->second);
				callbacks[name + suffix] = cbID;
			}
			else {
				callbacks.insert({ name + suffix, cbID });
			}
		}
		else {
			cout << "Error inserting callback: " + dn.name() + "\n";
		}
		if (node.hasFn(MFn::kMesh)) {

			cbID = MNodeMessage::addAttributeChangedCallback(node, nodeAttributeChanged, NULL, &status);
			if (status == MS::kSuccess) {
				string name = dn.name().asChar();
				string suffix = "AttributeChanged";
				auto itr = callbacks.find(name + suffix);
				if (itr != callbacks.end()) {
					cout << "Callback erased: " + itr->first + "\n";
					MMessage::removeCallback(itr->second);
					callbacks.erase(itr);
				}
				callbacks.insert({ name + suffix, cbID });
			}
			else {
				cout << "Error inserting callback: " + dn.name() + "\n";
			}
		}
		if (node.hasFn(MFn::kTransform)) {

			cbID = MNodeMessage::addAttributeChangedCallback(node, nodeTransformChanged, NULL, &status);
			if (status == MS::kSuccess) {
				string name = dn.name().asChar();
				string suffix = "TransformChanged";
				auto itr = callbacks.find(name + suffix);
				if (itr != callbacks.end()) {
					cout << "Callback erased: " + itr->first + "\n";
					MMessage::removeCallback(itr->second);
					callbacks.erase(itr);
				}
				callbacks.insert({ name + suffix, cbID });
			}
			else {
				cout << "Error inserting callback: " + dn.name() + "\n";
			}
		}
	}
}

void timerCallback(float elapsedTime, float lastTime, void* clientData) {
	totaltime += elapsedTime;
	fiveSecondMark = true;
	//cout << round(totaltime) << " seconds have elapsed\n";
}

void messageThread() {
	cout << "thread 2" << endl;
	while (!endThread) {
		if (fiveSecondMark) {
			//cout << "Thread 2 five seconds passed\n";
			fiveSecondMark = false;
		}
	}
	cout << "thread 2 terminated" << endl;
}


/*
 * Plugin entry point
 * For remote control of maya
 * open command port: commandPort -nr -name ":1234"
 * close command port: commandPort -cl -name ":1234"
 * send command: see loadPlugin.py and unloadPlugin.py
 */
EXPORT MStatus initializePlugin(MObject obj) {

	MStatus res = MS::kSuccess;

	MFnPlugin myPlugin(obj, "level editor", "1.0", "Any", &res);

	if (MFAIL(res)) {
		CHECK_MSTATUS(res);
		return res;
	}

	// redirect cout to cerr, so that when we do cout goes to cerr
	// in the maya output window (not the scripting output!)
	std::cout.set_rdbuf(MStreamUtils::stdOutStream().rdbuf());
	std::cerr.set_rdbuf(MStreamUtils::stdErrorStream().rdbuf());
	cout << "Plugin successfully loaded ==============" << endl;
	cout << "Adding callbacks to existing nodes ======" << endl;

	MItDependencyNodes dnitr;

	while (!dnitr.isDone())
	{
		nodeAdded(dnitr.thisNode(), nullptr);
		dnitr.next();
	}

	producerBuffer = new Comlib(L"Filemap", 150, ProcessType::Producer);

	// register callbacks here for
	auto nodeAddedId = MDGMessage::addNodeAddedCallback(nodeAdded, "dependNode", NULL, &status);
	if (status == MS::kSuccess) {
		callbacks.insert({ "nodeAdded", nodeAddedId });
	}
	else {
		cout << "Error inserting nodeAdded callback\n";
	}

	auto timerID = MTimerMessage::addTimerCallback(5, timerCallback, NULL, &status);
	if (status == MS::kSuccess) {
		callbacks.insert({ "timer", timerID });
	}
	else {
		cout << "Error inserting timer callback\n";
	}

	//msgThread = thread(messageThread);
	// a handy timer, courtesy of Maya
	gTimer.clear();
	gTimer.beginTimer();

	return res;
}


EXPORT MStatus uninitializePlugin(MObject obj) {
	MFnPlugin plugin(obj);

	cout << "Plugin unloaded =========================" << endl;
	endThread = true;
	//msgThread.join();

	cout << "Removing callbacks: \n";
	for (auto i : callbacks) {
		cout << i.first + '\n';
		MMessage::removeCallback(i.second);
	}

	return MS::kSuccess;
}
