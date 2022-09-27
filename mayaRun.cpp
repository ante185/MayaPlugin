#include "maya_includes.h"
#include <maya/MTimer.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <queue>
#include <unordered_map>

using namespace std;
//MCallbackIdArray callbackIdArray;
unordered_map<string, MCallbackId> callbacks;
MObject m_node;
MStatus status = MS::kSuccess;
bool initBool = false;

enum NODE_TYPE { TRANSFORM, MESH };
MTimer gTimer;
// keep track of created meshes to maintain them
queue<MObject> newMeshes;


void nodeAttributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void* x)
{
	//if (msg & MNodeMessage::AttributeMessage::kConnectionMade) {
	//	if (plug.node().hasFn(MFn::kTransform)) {
	//		MFnTransform firstNode(plug.node(), &status);
	//		if (status == MS::kSuccess) {
	//			cout << "First - we are here: " << firstNode.name() << endl;
	//			MPlugArray connections;
	//			MPlug firstNodePlugs = firstNode.findPlug("translate", &status);
	//			if (status == MS::kSuccess) {
	//				firstNodePlugs.connectedTo(connections, true, false);
	//			}
	//			for (size_t i = 0; i < connections.length(); i++) {
	//				if (connections[i].node().hasFn(MFn::kTransform)) {
	//					MFnTransform secondNode(connections[i].node(), &status);
	//					if (status == MS::kSuccess) {
	//						cout << "Now - we are here: " << secondNode.name() << endl;
	//					}
	//				}
	//			}
	//		}
	//	}
	//}

	if (plug.node().hasFn(MFn::kMesh)) {
		MFnMesh mesh(plug.node());
		if (mesh.name().length() > 0) {
			cout << "Modified: " << mesh.name() << endl;
			MPointArray pointArray;
			mesh.getPoints(pointArray, MSpace::kObject);
			cout << pointArray << endl;
		}
	}

}


void nodeNameChanged(MObject& node, const MString& str, void* clientData)
{
	if (node.hasFn(MFn::kDependencyNode)) {
		MFnDependencyNode dn(node);
		if (status == MS::kSuccess /*&& strcmp(str.asChar(), dn.name().asChar()) != 0*/) {
			cout << "Node " + str + " has been renamed " + dn.name() << endl;
		}
	}
}

void dirtyNode(MObject& node, void* clientData)
{
	if (node.hasFn(MFn::kTransform)) {
		MFnTransform tform(node);
		//cout << tform.name() << " suspects their transform is changed" << endl;
		auto transform = tform.transformation();
		if (status == MS::kSuccess) {
			cout << "Local transform: " << transform.asMatrix() << endl << endl;
		}
	}
}
/*
 * how Maya calls this method when a node is added.
 * new POLY mesh: kPolyXXX, kTransform, kMesh
 * new MATERIAL : kBlinn, kShadingEngine, kMaterialInfo
 * new LIGHT    : kTransform, [kPointLight, kDirLight, kAmbientLight]
 * new JOINT    : kJoint
 */
void nodeAdded(MObject &node, void * clientData) {
	if (node.hasFn(MFn::kDependencyNode)) {
		MFnDependencyNode dn(node);
		cout << "Node added: " << dn.name() << endl;
		MCallbackId cbID = MNodeMessage::addNameChangedCallback(node, nodeNameChanged, NULL, &status);
		if (status == MS::kSuccess) {
			string name = dn.name().asChar();
			string suffix = "NameChanged";
			auto itr = callbacks.find(name+suffix);
			if (itr != callbacks.end()) {
				cout<< "Callback erased: " + itr->first + "\n";
				MMessage::removeCallback(itr->second);
				callbacks.erase(itr);
			}
			callbacks.insert({name + suffix, cbID });
		}
		else {
			cout << "Error inserting callback: " + dn.name() + "\n";
		}
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
		if (node.hasFn(MFn::kTransform)) {

			cbID = MNodeMessage::addNodeDirtyCallback(node, dirtyNode, NULL, &status);
			if (status == MS::kSuccess) {
				string name = dn.name().asChar();
				string suffix = "Dirty";
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
	//cout << "5 seconds have elapsed\n";
	//Look it's annoying to have this going while testing other things
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
		callbacks.insert({"nodeAdded", nodeAddedId});
	}
	else {
		cout << "Error inserting nodeAdded callback\n";
	}

	// MDGMessage::addNodeRemovedCallback(nodeRemoved, "dependNode", NULL, &status);
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

	//MMessage::removeCallbacks(callbackIdArray);
	cout << "Removing callbacks: \n";
	for (auto i : callbacks) {
		cout << i.first + '\n';
		MMessage::removeCallback(i.second);
	}

	return MS::kSuccess;
}