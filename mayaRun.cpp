#include "maya_includes.h"
#include <maya/MTimer.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <queue>
#include <unordered_map>

using namespace std;
unordered_map<string, MCallbackId> callbacks;
MObject m_node;
MStatus status = MS::kSuccess;
bool initBool = false;

enum NODE_TYPE { TRANSFORM, MESH };
MTimer gTimer;
// keep track of created meshes to maintain them
queue<MObject> newMeshes;
static double totaltime = 0.0;


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
	}
}


void nodeNameChanged(MObject& node, const MString& str, void* clientData)
{
	if (node.hasFn(MFn::kDependencyNode)) {
		MFnDependencyNode dn(node);
		if (status == MS::kSuccess /*&& strcmp(str.asChar(), dn.name().asChar()) != 0*/) {
			cout << "Node " + str + " has been renamed to " + dn.name() << endl;
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
		cout << "Node added: " << dn.name() << endl;
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
	cout << round(totaltime) << " seconds have elapsed\n";
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

	// a handy timer, courtesy of Maya
	gTimer.clear();
	gTimer.beginTimer();

	return res;
}


EXPORT MStatus uninitializePlugin(MObject obj) {
	MFnPlugin plugin(obj);

	cout << "Plugin unloaded =========================" << endl;

	cout << "Removing callbacks: \n";
	for (auto i : callbacks) {
		cout << i.first + '\n';
		MMessage::removeCallback(i.second);
	}

	return MS::kSuccess;
}