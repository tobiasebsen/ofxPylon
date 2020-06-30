#include <pylon/PylonIncludes.h>
#include <pylon/BaslerUniversalInstantCamera.h>

#include "Camera.h"


class ImageEventHandler : public Pylon::CImageEventHandler {
public:
	ImageEventHandler(ofxPylon::Camera * c) : parent(c) {}

	void OnImageGrabbed(Pylon::CInstantCamera&, const Pylon::CGrabResultPtr& grabResult) {
		std::lock_guard<std::mutex> locker(parent->lock);

		try {

			size_t width = grabResult->GetWidth();
			size_t height = grabResult->GetHeight();

			Pylon::EPixelType pixelType = grabResult->GetPixelType();
			void * buffer = grabResult->GetBuffer();

			if (pixelType == Pylon::PixelType::PixelType_Mono8) {
				parent->pixelsFront->allocate(width, height, 1);
				parent->pixelsFront->setFromPixels(static_cast<unsigned char*>(buffer), width, height, 1);
				parent->frontGrabbed = true;
			}
			if (pixelType == Pylon::PixelType::PixelType_RGB8packed) {
				parent->pixelsFront->allocate(width, height, 3);
				parent->pixelsFront->setFromPixels(static_cast<unsigned char*>(buffer), width, height, 3);
				parent->frontGrabbed = true;
			}
			else {
				parent->pixelsFront->allocate(width, height, IsMonoImage(pixelType) ? 1 : 3);
				converter.OutputPixelFormat = Pylon::IsMonoImage(pixelType) ? Pylon::PixelType::PixelType_Mono8 : Pylon::PixelType_RGB8packed;
				converter.Convert((void*)parent->pixelsFront->getData(), parent->pixelsFront->getTotalBytes(), grabResult);
				parent->frontGrabbed = true;
			}
		}
		catch (Pylon::GenericException & e) {
			ofLogError("ofxPylon::Camera") << e.GetDescription();
		}
	}

private:
	ofxPylon::Camera * parent;
	Pylon::CImageFormatConverter converter;
};



ofxPylon::Camera::Camera() {

	Pylon::PylonInitialize();

	configHandler = shared_ptr<Pylon::CAcquireContinuousConfiguration>(new Pylon::CAcquireContinuousConfiguration);
	imageHandler = shared_ptr<Pylon::CImageEventHandler>(new ImageEventHandler(this));

	ofAddListener(paramGroup.parameterChangedE(), this, &ofxPylon::Camera::parameterChanged);
}

ofxPylon::Camera::~Camera() {

	close();

	imageHandler.reset();
	configHandler.reset();

	Pylon::PylonTerminate();
}

std::vector<ofVideoDevice> ofxPylon::Camera::listDevices() const {

	std::vector<ofVideoDevice> devices;

	Pylon::DeviceInfoList_t list;
	Pylon::CTlFactory::GetInstance().EnumerateDevices(list);

	for (auto & info : list) {
		ofVideoDevice device;
		device.id = ofToInt(info.GetDeviceID().c_str());
		device.deviceName = info.GetModelName();
		device.hardwareName = info.GetVendorName();
	}

	return devices;
}

bool ofxPylon::Camera::open() {

	close();

	try {
		camera = shared_ptr<Pylon::CInstantCamera>(new Pylon::CInstantCamera(Pylon::CTlFactory::GetInstance().CreateFirstDevice()));
		camera->RegisterConfiguration(configHandler.get(), Pylon::RegistrationMode_ReplaceAll, Pylon::Ownership_ExternalOwnership);
		camera->RegisterImageEventHandler(imageHandler.get(), Pylon::RegistrationMode_ReplaceAll, Pylon::Ownership_ExternalOwnership);
		camera->Open();
	}
	catch (Pylon::GenericException & e) {
		ofLogError("ofxPylon::Camera") << e.GetDescription();
		return false;
	}

	return true;
}

bool ofxPylon::Camera::start() {

	if (!camera || camera->IsGrabbing())
		return false;

	try {
		camera->StartGrabbing(Pylon::GrabStrategy_OneByOne, Pylon::GrabLoop_ProvidedByInstantCamera);
	}
	catch (Pylon::GenericException & e) {
		ofLogError("ofxPylon::Camera") << e.GetDescription();
		return false;
	}
	return true;
}

void ofxPylon::Camera::stop() {

	if (camera) {
		std::lock_guard<std::mutex> locker(lock);

		if (!camera->IsGrabbing())
			return;

		try {
			camera->StopGrabbing();
		}
		catch (Pylon::GenericException & e) {
			ofLogError("ofxPylon::Camera") << e.GetDescription();
		}
	}
}

bool ofxPylon::Camera::setup() {
	open();
	start();
	return true;
}

bool ofxPylon::Camera::setup(int w, int h) {

	if (!camera && !open()) {
		return false;
	}

	try {

		GenApi::INodeMap & nodeMap = camera->GetNodeMap();
		uint64_t widthMax = Pylon::CIntegerParameter(nodeMap.GetNode("SensorWidth")).GetValue();
		uint64_t heightMax = Pylon::CIntegerParameter(nodeMap.GetNode("SensorHeight")).GetValue();

		Pylon::CIntegerParameter(nodeMap.GetNode("Width")).SetValue(w);
		Pylon::CIntegerParameter(nodeMap.GetNode("Height")).SetValue(h);

		Pylon::CBooleanParameter(nodeMap.GetNode("CenterX")).SetValue(1);
		Pylon::CBooleanParameter(nodeMap.GetNode("CenterY")).SetValue(1);


		uint64_t binH = floor(widthMax / w);
		uint64_t binV = floor(heightMax / h);

		Pylon::CIntegerParameter(nodeMap.GetNode("BinningHorizontal")).SetValue(binH);
		Pylon::CIntegerParameter(nodeMap.GetNode("BinningVertical")).SetValue(binV);

	}
	catch (Pylon::GenericException & e) {
		ofLogError("ofxPylon::Camera") << e.GetDescription();
	}

	return start();
}

bool ofxPylon::Camera::setup(int w, int h, bool useTexture) {
	this->useTexture = useTexture;
	return setup(w, h);
}

void ofxPylon::Camera::close() {
	if (camera) {
		std::lock_guard<std::mutex> locker(lock);

		camera->StopGrabbing();

		camera->Close();
		camera.reset();
	}
}

float ofxPylon::Camera::getWidth() const {
	return camera != NULL ? Pylon::CIntegerParameter(camera->GetNodeMap().GetNode("Width")).GetValue() : 0.0f;
}

float ofxPylon::Camera::getHeight() const {
	return camera != NULL ? Pylon::CIntegerParameter(camera->GetNodeMap().GetNode("Height")).GetValue() : 0.0f;
}

bool ofxPylon::Camera::isFrameNew() const {
	return frameNew;
}

bool ofxPylon::Camera::isInitialized() const {
	return camera != NULL ? camera->IsPylonDeviceAttached() : false;
}

bool ofxPylon::Camera::setPixelFormat(ofPixelFormat pixelFormat) {
	return false;
}

ofPixelFormat ofxPylon::Camera::getPixelFormat() const {
	return pixelsBack->getPixelFormat();
}

ofPixels & ofxPylon::Camera::getPixels() {
	return *pixelsBack;
}

const ofPixels & ofxPylon::Camera::getPixels() const {
	return *pixelsBack;
}

void ofxPylon::Camera::update() {
	std::unique_lock<std::mutex> locker(lock, std::defer_lock);
	frameNew = false;

	if (frontGrabbed && locker.try_lock()) {
		swap(pixelsBack, pixelsFront);
		frontGrabbed = false;
		frameNew = true;
		locker.unlock();

		if (useTexture) {
			texture.loadData(getPixels());
		}
	}
}

void ofxPylon::Camera::draw(float x, float y, float w, float h) const {
	if (texture.isAllocated()) {
		texture.draw(x, y, w, h);
	}
}

std::vector<ofTexture>& ofxPylon::Camera::getTexturePlanes() {
	return texturePlanes;
}

const std::vector<ofTexture>& ofxPylon::Camera::getTexturePlanes() const {
	return texturePlanes;
}

ofTexture & ofxPylon::Camera::getTexture() {
	return texture;
}

const ofTexture & ofxPylon::Camera::getTexture() const {
	return texture;
}

void ofxPylon::Camera::setUseTexture(bool bUseTex) {
	useTexture = bUseTex;
}

bool ofxPylon::Camera::isUsingTexture() const {
	return useTexture;
}

ofParameter<float> & ofxPylon::Camera::getParameterFloat(string name) {
	auto param = paramGroup.contains(name) ? paramGroup.getFloat(name) : ofParameter<float>(name, 0);

	if (camera && camera->IsOpen()) {
		GenApi::INodeMap & nodeMap = camera->GetNodeMap();
		GenApi::INode * node = nodeMap.GetNode(name.c_str());
		auto & p = Pylon::CFloatParameter(node);
		param.set(p.GetValue());
		param.setMin(p.GetMin());
		param.setMax(p.GetMax());
	}

	if (!paramGroup.contains(name)) {
		paramGroup.add(param);
	}
	return paramGroup.getFloat(name);
}

ofParameter<string>& ofxPylon::Camera::getParameterString(string name) {
	auto param = paramGroup.contains(name) ? paramGroup.getString(name) : ofParameter<string>(name, "");

	if (camera && camera->IsOpen()) {
		GenApi::INodeMap & nodeMap = camera->GetNodeMap();
		GenApi::INode * node = nodeMap.GetNode(name.c_str());
		auto & p = Pylon::CStringParameter(node);
		param.set(p.GetValue().c_str());
	}

	if (!paramGroup.contains(name)) {
		paramGroup.add(param);
	}
	return paramGroup.getString(name);
}

const ofParameterGroup & ofxPylon::Camera::getParameterGroup() const {
	return paramGroup;
}

void ofxPylon::Camera::parameterChanged(ofAbstractParameter & ap) {
	if (!camera || !camera->IsOpen())
		return;
	GenApi::INodeMap & nodeMap = camera->GetNodeMap();
	GenApi::INode * node = nodeMap.GetNode(ap.getName().c_str());
	GenApi::EInterfaceType itype = node->GetPrincipalInterfaceType();
	if (itype == GenApi::EInterfaceType::intfIFloat) {
		auto & p = Pylon::CFloatParameter(node);
		try {
			p.SetValue(ap.cast<float>().get());
		}
		catch (Pylon::GenericException & e) {
			ofLogError("ofxPylon::Camera") << e.GetDescription();
		}
	}
}
