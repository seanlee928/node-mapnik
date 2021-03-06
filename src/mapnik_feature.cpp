#include "utils.hpp"
#include "mapnik_feature.hpp"
#include "mapnik_geometry.hpp"

// mapnik
#include <mapnik/unicode.hpp>
#include <mapnik/feature_factory.hpp>
#include <mapnik/feature_kv_iterator.hpp>

// boost
#include <boost/scoped_ptr.hpp>
#include <boost/make_shared.hpp>

Persistent<FunctionTemplate> Feature::constructor;

void Feature::Initialize(Handle<Object> target) {

    HandleScope scope;

    constructor = Persistent<FunctionTemplate>::New(FunctionTemplate::New(Feature::New));
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(String::NewSymbol("Feature"));
    
    NODE_SET_PROTOTYPE_METHOD(constructor, "id", id);
    NODE_SET_PROTOTYPE_METHOD(constructor, "extent", extent);
    NODE_SET_PROTOTYPE_METHOD(constructor, "attributes", attributes);
    NODE_SET_PROTOTYPE_METHOD(constructor, "addGeometry", addGeometry);
    NODE_SET_PROTOTYPE_METHOD(constructor, "addAttributes", addAttributes);
    NODE_SET_PROTOTYPE_METHOD(constructor, "numGeometries", numGeometries);
    NODE_SET_PROTOTYPE_METHOD(constructor, "toString", toString);
    target->Set(String::NewSymbol("Feature"),constructor->GetFunction());
}

Feature::Feature(mapnik::feature_ptr f) :
  ObjectWrap(),
  this_(f) {}

Feature::Feature(int id) :
  ObjectWrap(),
  this_() {
    // TODO - accept/require context object to reused
    ctx_ = boost::make_shared<mapnik::context_type>();
    this_ = mapnik::feature_factory::create(ctx_,id);
  }

Feature::~Feature()
{
}

Handle<Value> Feature::New(const Arguments& args)
{
    HandleScope scope;

    if (!args.IsConstructCall())
        return ThrowException(String::New("Cannot call constructor as function, you need to use 'new' keyword"));

    if (args[0]->IsExternal())
    {
        Local<External> ext = Local<External>::Cast(args[0]);
        void* ptr = ext->Value();
        Feature* f =  static_cast<Feature*>(ptr);
        f->Wrap(args.This());
        return args.This();
    }

    // TODO - expose mapnik.Context
    
    if (!args.Length() == 1 || !args[0]->IsNumber()) {
        return ThrowException(Exception::TypeError(
          String::New("requires one argument: an integer feature id")));
    }
    
    Feature* f = new Feature(args[0]->IntegerValue());
    f->Wrap(args.This());
    return args.This();
}

Handle<Value> Feature::New(mapnik::feature_ptr f_ptr)
{
    HandleScope scope;
    Feature* f = new Feature(f_ptr);
    Handle<Value> ext = External::New(f);
    Handle<Object> obj = constructor->GetFunction()->NewInstance(1, &ext);
    return scope.Close(obj);
}

Handle<Value> Feature::id(const Arguments& args)
{
    HandleScope scope;

    Feature* fp = ObjectWrap::Unwrap<Feature>(args.This());
    // TODO - provide custom 64 bit integer type?
    return scope.Close(Integer::New(fp->get()->id()));
}

Handle<Value> Feature::extent(const Arguments& args)
{
    HandleScope scope;

    Feature* fp = ObjectWrap::Unwrap<Feature>(args.This());

    Local<Array> a = Array::New(4);
    mapnik::box2d<double> const& e = fp->get()->envelope();
    a->Set(0, Number::New(e.minx()));
    a->Set(1, Number::New(e.miny()));
    a->Set(2, Number::New(e.maxx()));
    a->Set(3, Number::New(e.maxy()));
 
    return scope.Close(a);
}

Handle<Value> Feature::attributes(const Arguments& args)
{
    HandleScope scope;

    Feature* fp = ObjectWrap::Unwrap<Feature>(args.This());
    
    Local<Object> feat = Object::New();

    mapnik::feature_ptr feature = fp->get();
    mapnik::feature_kv_iterator itr = feature->begin();
    mapnik::feature_kv_iterator end = feature->end();
    for ( ;itr!=end; ++itr)
    {
        node_mapnik::params_to_object serializer( feat , boost::get<0>(*itr));
        boost::apply_visitor( serializer, boost::get<1>(*itr).base() );
    }
    
    return scope.Close(feat);
}

Handle<Value> Feature::numGeometries(const Arguments& args)
{
    HandleScope scope;
    Feature* fp = ObjectWrap::Unwrap<Feature>(args.This());
    return scope.Close(Integer::New(fp->get()->num_geometries()));
}

// TODO void?
Handle<Value> Feature::addGeometry(const Arguments& args)
{
    HandleScope scope;

    Feature* fp = ObjectWrap::Unwrap<Feature>(args.This());

    if (args.Length() >= 1 ) {
        Local<Value> value = args[0];
        if (value->IsNull() || value->IsUndefined()) {
            return ThrowException(Exception::TypeError(String::New("mapnik.Geometry instance expected")));
        } else {
            Local<Object> obj = value->ToObject();
            if (Geometry::constructor->HasInstance(obj)) {
                Geometry* g = ObjectWrap::Unwrap<Geometry>(obj);
            
                try
                {
                    std::auto_ptr<mapnik::geometry_type> geom_ptr = g->get();
                    if (geom_ptr.get()) {
                        fp->get()->add_geometry(geom_ptr.get());
                        geom_ptr.release();
                    } else {
                        return ThrowException(Exception::Error(
                          String::New("empty geometry!")));
                    }
                }
                catch (const std::exception & ex )
                {
                    return ThrowException(Exception::Error(
                      String::New(ex.what())));
                }
                catch (...) {
                    return ThrowException(Exception::Error(
                      String::New("Unknown exception happended - please report bug")));
                }
            }
        }
    }

    return Undefined();
}

Handle<Value> Feature::addAttributes(const Arguments& args)
{
    HandleScope scope;

    Feature* fp = ObjectWrap::Unwrap<Feature>(args.This());

    if (args.Length() > 0 ) {
        Local<Value> value = args[0];
        if (value->IsNull() || value->IsUndefined()) {
            return ThrowException(Exception::TypeError(String::New("object expected")));
        } else {
            Local<Object> attr = value->ToObject();
            try
            {
                Local<Array> names = attr->GetPropertyNames();
                uint32_t i = 0;
                uint32_t a_length = names->Length();
                boost::scoped_ptr<mapnik::transcoder> tr(new mapnik::transcoder("utf8"));
                while (i < a_length) {
                    Local<Value> name = names->Get(i)->ToString();
                    Local<Value> value = attr->Get(name);
                    if (value->IsString()) {
                        UnicodeString ustr = tr->transcode(TOSTR(value));
                        fp->get()->put_new(TOSTR(name),ustr);
                    } else if (value->IsNumber()) {
                        double num = value->NumberValue();
                        // todo - round
                        if (num == value->IntegerValue()) {
                            int integer = value->IntegerValue();
                            fp->get()->put_new(TOSTR(name),integer);
                        } else {
                            double dub_val = value->NumberValue();
                            fp->get()->put_new(TOSTR(name),dub_val);
                        }
                    } else {
                        std::clog << "unhandled type for property: " << TOSTR(name) << "\n";
                    }
                    i++;
                }
            }
            catch (const std::exception & ex )
            {
                return ThrowException(Exception::Error(
                  String::New(ex.what())));
            }
            catch (...) {
                return ThrowException(Exception::Error(
                  String::New("Unknown exception happended - please report bug")));
            }
        }
    }

    return Undefined();
}

Handle<Value> Feature::toString(const Arguments& args)
{
    HandleScope scope;

    Feature* fp = ObjectWrap::Unwrap<Feature>(args.This());
    return scope.Close(String::New(fp->get()->to_string().c_str()));
}


