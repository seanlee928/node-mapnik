#ifndef __NODE_MAPNIK_COLOR_H__
#define __NODE_MAPNIK_COLOR_H__

#include <v8.h>
#include <node.h>
#include <node_object_wrap.h>

// mapnik
#include <mapnik/color.hpp>

// boost
#include <boost/shared_ptr.hpp>

using namespace v8;
using namespace node;

typedef boost::shared_ptr<mapnik::color> color_ptr;

class Color: public node::ObjectWrap {
  public:
    static Persistent<FunctionTemplate> constructor;
    static void Initialize(Handle<Object> target);
    static Handle<Value> New(const Arguments &args);
    static Handle<Value> New(mapnik::color const& color);
    static Handle<Value> toString(const Arguments &args);
    static Handle<Value> hex(const Arguments &args);

    static Handle<Value> get_prop(Local<String> property,
                         const AccessorInfo& info);
    static void set_prop(Local<String> property,
                         Local<Value> value,
                         const AccessorInfo& info);
    Color();
    inline color_ptr get() { return this_; }

  private:
    ~Color();
    color_ptr this_;
};

#endif
