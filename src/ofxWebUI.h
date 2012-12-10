#pragma once

#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/ServerSocket.h"

#include "ofEvents.h"

#include "ofxWebUIRequestHandler.h"

template<class T>
class ofxWebUI
{
public:
  ofxWebUI(T& _pb, Poco::Net::ServerSocket& _serverSocket)
  : pb(_pb)
  , serverSocket(_serverSocket)
  , webUIRequestHandlerFactory(_pb, onReceivedMessageEvent, onHttpEvent)
  , server(&webUIRequestHandlerFactory, _serverSocket, new Poco::Net::HTTPServerParams)
  {
    server.start();
  }

  ~ofxWebUI()
  {
    server.stop();
  }

  ofEvent<const T> onReceivedMessageEvent;
  ofEvent<const HttpEventArgs> onHttpEvent;

private:
  T& pb;
//  unsigned short port;
  Poco::Net::HTTPServer server;
  Poco::Net::ServerSocket& serverSocket;
  ofxWebUIRequestHandlerFactory<T> webUIRequestHandlerFactory;
};
