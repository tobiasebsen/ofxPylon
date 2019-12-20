#pragma once
#include "ofMain.h"

namespace Pylon {
	class CInstantCamera;
	class CConfigurationEventHandler;
	class CImageEventHandler;
}
class ImageEventHandler;

namespace ofxPylon {

	class Camera : public ofBaseVideoGrabber, public ofBaseVideoDraws {
	public:
		Camera();
		~Camera();

		bool open();
		void start();
		void stop();

		// ofBaseVideoGrabber
		virtual std::vector<ofVideoDevice> listDevices() const;
		virtual bool setup();
		virtual bool setup(int w, int h);
		virtual bool setup(int w, int h, bool useTexture);
		virtual float getHeight() const;
		virtual float getWidth() const;

		// ofBaseVideo
		virtual bool isFrameNew() const;
		virtual void close();
		virtual bool isInitialized() const;
		virtual bool setPixelFormat(ofPixelFormat pixelFormat);
		virtual ofPixelFormat getPixelFormat() const;

		// ofBaseHasPixels
		virtual ofPixels & getPixels();
		virtual const ofPixels & getPixels() const;

		// ofBaseUpdates
		virtual void update();

		// ofBaseDraws
		virtual void draw(float x, float y, float w, float h) const;

		// ofBaseHasTexturePlanes
		virtual std::vector<ofTexture> & getTexturePlanes();
		virtual const std::vector<ofTexture> & getTexturePlanes() const;

		// ofBaseHasTexture
		virtual ofTexture & getTexture();
		virtual const ofTexture & getTexture() const;
		virtual void setUseTexture(bool bUseTex);
		virtual bool isUsingTexture() const;

	protected:
		friend ImageEventHandler;
		std::mutex lock;

		ofPixels pixels[2];
		ofPixels * pixelsFront = &pixels[0];
		ofPixels * pixelsBack = &pixels[1];
		bool frontGrabbed = false;

	private:
		shared_ptr<Pylon::CInstantCamera> camera;
		shared_ptr<Pylon::CConfigurationEventHandler> configHandler;
		shared_ptr<Pylon::CImageEventHandler> imageHandler;

		bool frameNew = false;

		ofTexture texture;
		vector<ofTexture> texturePlanes = { 1 , texture };
		bool useTexture = true;
	};
}