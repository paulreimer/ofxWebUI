#pragma once

#include "Poco/Net/WebSocket.h"
#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/NetException.h"

#include <Poco/Base64Decoder.h>
#include <Poco/Base64Encoder.h>

#include "ofEvents.h"
#include "ofFileUtils.h"
#include "ofUtils.h"

class HttpEventArgs
{
public:
  HttpEventArgs(const Poco::Net::HTTPServerRequest& _request)
  : request(_request)
  {}

  const Poco::Net::HTTPServerRequest& request;
};

template<class T>
class ofxWebUIRequestHandler
: public Poco::Net::HTTPRequestHandler
{
public:
  ofxWebUIRequestHandler(T& _pb,
                         ofEvent<const T>& _onReceivedMessageEvent,
                         ofEvent<const HttpEventArgs>& _onHttpEvent)
  : pb(_pb)
  , onReceivedMessageEvent(_onReceivedMessageEvent)
  , onHttpEvent(_onHttpEvent)
  {}
  
  virtual ~ofxWebUIRequestHandler()
  {}

  void handleRequest(Poco::Net::HTTPServerRequest& request,
                     Poco::Net::HTTPServerResponse& response)
  {
    std::string url = request.getURI();
    if (url.empty() || url == "/")
      url = "/index.html";

    std::string targetFilePath = ofToDataPath("web"+url, true);
    if (ofFile::doesFileExist(targetFilePath))
    {
      std::string mimetype = "application/octet-stream";
      std::string ext = ofFilePath::getFileExt(targetFilePath);
      if (ext == "html")
        mimetype = "text/html";
      else if (ext == "js")
        mimetype = "text/javascript";

      response.sendFile(targetFilePath, mimetype);
    }
    else if (url == "/ws" )
    {
      try
      {
        std::string protocol = request.get("Sec-WebSocket-Protocol");

        Poco::Net::WebSocket ws(request, response);
        char buffer[1024];
        int flags;
        int n;

        // send new client the current state
        std::string serializedPb;
        pb.SerializeToString(&serializedPb);
        
        bool useBase64 = (protocol.empty() || protocol != "pb-binary");
        if (useBase64)
        {
          std::ostringstream oss;
          Poco::Base64Encoder encoder(oss);

          encoder << serializedPb;
          std::string encodedString = oss.str();

          if (!encodedString.empty())
            ws.sendFrame(encodedString.c_str(), encodedString.size(), Poco::Net::WebSocket::FRAME_TEXT);
        }
        else
          ws.sendFrame(serializedPb.data(), serializedPb.size(), Poco::Net::WebSocket::FRAME_BINARY);

        do
        {
          n = ws.receiveFrame(buffer, sizeof(buffer), flags);
          if (n > 0)
          {
            T pbDiff;
            bool validDiff = false;
            if ((flags & Poco::Net::WebSocket::FRAME_BINARY) && !useBase64)
            {
              validDiff = pbDiff.ParseFromArray(buffer, n);
            }
            else {
              std::istringstream iss(std::string(buffer, n));
              Poco::Base64Decoder decoder(iss);
              
              std::string decodedString(n, 0x00);
              decoder.read(&decodedString[0], decodedString.size());
              validDiff = pbDiff.ParseFromArray(&decodedString[0], decoder.gcount());
            }

            if (validDiff)
            {
              ofNotifyEvent(onReceivedMessageEvent, pbDiff);
            }
          }
        }
        while (n > 0 || (flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) != Poco::Net::WebSocket::FRAME_OP_CLOSE);
      }
      catch (Poco::Net::WebSocketException& exc)
      {
        switch (exc.code())
        {
          case Poco::Net::WebSocket::WS_ERR_HANDSHAKE_UNSUPPORTED_VERSION:
            response.set("Sec-WebSocket-Version", Poco::Net::WebSocket::WEBSOCKET_VERSION);
            // fallthrough
          case Poco::Net::WebSocket::WS_ERR_NO_HANDSHAKE:
          case Poco::Net::WebSocket::WS_ERR_HANDSHAKE_NO_VERSION:
          case Poco::Net::WebSocket::WS_ERR_HANDSHAKE_NO_KEY:
            response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            response.setContentLength(0);
            response.send();
            break;
        }
      }
    }
    else
    {
      ofNotifyEvent(onHttpEvent, HttpEventArgs(request));
//      response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
      response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
      response.send();
    }
  }
  
  T& pb;
  ofEvent<const T>& onReceivedMessageEvent;
  ofEvent<const HttpEventArgs>& onHttpEvent;
};

template<class T>
class ofxWebUIRequestHandlerFactory
: public Poco::Net::HTTPRequestHandlerFactory
{
public:
  ofxWebUIRequestHandlerFactory(T& _pb,
                                ofEvent<const T>& _onReceivedMessageEvent)
  : pb(_pb)
  , onReceivedMessageEvent(_onReceivedMessageEvent)
  {}

  Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request)
  {
    return new ofxWebUIRequestHandler<T>(pb, onReceivedMessageEvent, onHttpEvent);
  }
  
  T& pb;
  ofEvent<const T>& onReceivedMessageEvent;
  ofEvent<const HttpEventArgs> onHttpEvent;
};
