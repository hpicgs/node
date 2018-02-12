## How to use Node.js as a shared library
### Handling the Node.js event loop
There are two different ways of handling the Node.js event loop.
#### C++ keeps control over thread
By calling `node::lib::ProcessEvents()`, the Node.js event loop will be run once, handling the next pending event. The return value of the call specifies whether there are more events in the queue.

#### C++ gives up control of the thread to Node.js
By calling `node::lib::RunEventLoop(callback)`, the C++ host program gives up the control of the thread and allows the Node.js event loop to run until no more events are in the queue or `node::lib::StopEventLoop()` is called. The `callback` parameter in the `RunEventLoop` function is called once per event loop. This allows the C++ programmer to react on changes in the Node.js state and e.g. terminate Node.js preemptively.

### Examples

In the following, simple examples demonstrate the usage of Node.js as a library. For more complex examples, including handling of the event loop, see the [node-embed](https://github.com/hpicgs/node-embed) repository.

#### (1) Evaluating in-line JavaScript code
This example evaluates multiple lines of JavaScript code in the global Node context. The result of `console.log` is piped to the stdout.

```C++
node::lib::Initialize("example01");
node::lib::Evaluate("var helloMessage = 'Hello from Node.js!';");
node::lib::Evaluate("console.log(helloMessage);");
```

#### (2) Running a JavaScript file
This example evaluates a JavaScript file and lets Node handle all pending events until the event loop is empty.

```C++
node::lib::Initialize("example02");
node::lib::Run("cli.js");
while (node::lib::ProcessEvents()) { }
``` 


#### (3) Including an NPM Module
This example uses the [fs](https://nodejs.org/api/fs.html) module to check whether a specific file exists.
```C++
node::lib::Initialize("example03");
auto fs = node::lib::IncludeModule("fs");
v8::Isolate *isolate = node::lib::internal::isolate();

// Check if file cli.js exists in the current working directory.
auto result = node::lib::Call(fs, "existsSync", {v8::String::NewFromUtf8(isolate, "cli.js")});

auto file_exists = v8::Local<v8::Boolean>::Cast(result)->BooleanValue();
std::cout << (file_exists ? "cli.js exists in cwd" : "cli.js does NOT exist in cwd") << std::endl;

```

#### (4) Advanced!!

The following example demonstrates the usage of Node.js as a library by using the request and the feedreader module from NPM.

##### RssFeed.h
```C++
#pragma once

#include <QObject>
#include "node.h"

/**
 * @brief The RssFeed class retrieves an RSS feed from the Internet and provides its entries.
 */
class RssFeed : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList entries READ getEntries NOTIFY entriesChanged)

public:
    explicit RssFeed(QObject* parent=nullptr);
    static void clearFeed(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void cppLog(const v8::FunctionCallbackInfo<v8::Value>& args);
    static RssFeed& getInstance();
    static void redrawGUI(const v8::FunctionCallbackInfo<v8::Value>& args);
    Q_INVOKABLE static void refreshFeed();

private:
    static RssFeed* instance;
    QStringList entries;

signals:
    void entriesChanged();

public slots:
    /**
     * @brief getEntries returns the entries of the RSS feed
     * @return a list of text entries
     */
    QStringList getEntries() const;
};
```

##### RssFeed.cpp
```C++
#include "RssFeed.h"

#include <iostream>
#include <QGuiApplication>
#include "node_lib.h"

RssFeed* RssFeed::instance = nullptr;

RssFeed::RssFeed(QObject* parent)
    : QObject(parent)
{
   
}

RssFeed& RssFeed::getInstance(){
    if (instance == nullptr){
        instance = new RssFeed();
    }
    return *instance;
}

QStringList RssFeed::getEntries() const {
    return entries;
}

void RssFeed::clearFeed(const v8::FunctionCallbackInfo<v8::Value>& args) {
    getInstance().entries.clear();
}

void RssFeed::redrawGUI(const v8::FunctionCallbackInfo<v8::Value>& args) {
    emit getInstance().entriesChanged();
}

void RssFeed::refreshFeed() {
    node::Evaluate("emitRequest()");
    node::RunEventLoop([](){ QGuiApplication::processEvents(); });
}

void RssFeed::displayFeed(const v8::FunctionCallbackInfo<v8::Value>& args){
    v8::Isolate* isolate = args.GetIsolate();
    if (args.Length() < 1 || !args[0]->IsObject()) {
        isolate->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(isolate, "Error: One object expected")));
        return;
    }

    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Object> obj = args[0]->ToObject(context).ToLocalChecked();
    v8::Local<v8::Array> props = obj->GetOwnPropertyNames(context).ToLocalChecked();

    for (int i = 0, l = props->Length(); i < l; i++) {
        v8::Local<v8::Value> localKey = props->Get(i);
        v8::Local<v8::Value> localVal = obj->Get(context, localKey).ToLocalChecked();
        std::string key = *v8::String::Utf8Value(localKey);
        std::string val = *v8::String::Utf8Value(localVal);
        getInstance().entries << QString::fromStdString(val);
    }
}
```

##### main.cpp
```C++
#include <iostream>
#include <thread>
#include <chrono>

#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>

#include <cpplocate/cpplocate.h>

#include "node.h"
#include "node_lib.h"

#include "RssFeed.h"

int main(int argc, char* argv[]) {
    const std::string js_file = "data/node-lib-qt-rss.js";
    const std::string data_path = cpplocate::locatePath(js_file);
    const std::string js_path = data_path + "/" + js_file;

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    // to be able to access the public slots of the RssFeed instance
    // we inject a pointer to it in the QML context:
    engine.rootContext()->setContextProperty("rssFeed", &RssFeed::getInstance());

    engine.load(QUrl(QLatin1String("qrc:/main.qml")));

    node::lib::Initialize();
    node::lib::RegisterModule("cpp-qt-gui", {
                                {"refreshFeed", RssFeed::refreshFeed},
                                {"clearFeed", RssFeed::clearFeed},
                                {"redrawGUI", RssFeed::redrawGUI},
                              }, "cppQtGui");
    node::Run(js_path);
    RssFeed::refreshFeed();
    app.exec();
    node::Deinitialize();
}
```

```JS
var emitRequest = function () {
  console.log("Refreshing feeds...")
  var req = request('http://feeds.bbci.co.uk/news/world/rss.xml')

  req.on('error', function (error) {
    // handle any request errors
  });

  req.on('response', function (res) {
    cppQtGui.redrawGUI();
    
    var stream = this; // `this` is `req`, which is a stream
  
    if (res.statusCode !== 200) {
      this.emit('error', new Error('Bad status code'));
    }
    else {
      cppQtGui.clearFeed();
      stream.pipe(feedparser);
    }
  });
}

```