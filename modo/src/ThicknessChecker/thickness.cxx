// Example plugin which measures the projected thickness of all the polygons
// on a mesh.  A ray is fired from each polygon the opposite direction of that
// poly's normal, and if that ray hits another polygon on the mesh, the distance
// is compared to the user's minimum and maximum thickness values.  Polys with
// projected thicknesses more than the max get a red dot, polys below the minimum
// get a blue dot, and polys that hit nothing or fall within that range show no dots.

// This is slow during mesh edits and would need quite a bit of work to become performant,
// but it at least shows a few concepts for anyone interested and might be fun to play
// with on less complex meshes.

#include <lxsdk/lx_draw.hpp>
#include <lxsdk/lx_force.hpp>
#include <lxsdk/lx_handles.hpp>
#include <lxsdk/lx_item.hpp>
#include <lxsdk/lx_plugin.hpp>
#include <lxsdk/lx_thread.hpp>
#include <lxsdk/lx_vmodel.hpp>
#include <lxsdk/lxidef.h>
#include <lxsdk/lxu_math.hpp>
#include <lxsdk/lxu_modifier.hpp>
#include <lxsdk/lxu_package.hpp>
#include <lxsdk/lxu_schematic.hpp>
#include <lxsdk/lxu_value.hpp>

#include <array>
#include <cmath>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace servers
{
    static const std::string value    = "floatLists.value";
    static const std::string package  = "thick.maxMin";
    static const std::string modifier = "thick.maxMin.mod";
    static const std::string instance = "thick.maxMin.inst";
    static const std::string graph    = "thick.maxMin.graph";
}  // namespace servers

namespace channels
{
    static const std::string max      = "max";
    static const std::string min      = "min";
    static const std::string polylist = "polyList";
}  // namespace channels

// The first part of the plugin is a custom value.  We're going to store
// sets of the too-thick or too-thin polys as custom data on our item, and
// then tell modo's eval system that computing this value requires that the
// mesh geometry has been evaluated.  This ensures we don't end up drawing
// stale data or data that's in the middle of evaluation on another thread.
namespace polyListData
{
    using vectorList = std::vector<std::array<double, 3>>;

    // Custom data types can be very simple.  We need to wrap our data
    // in a class that inherits from CLxValue and implement the copy and
    // compare functions, then register it with a metaRoot object (the pattern)
    // used for all meta class plugins.
    class Value : public CLxValue
    {
    public:
        // We store two vectors of poly positions. One is for polys
        // with a projected thickness over the max value the user sets
        // and the other is for thicknesses under the min value.
        vectorList overMax;
        vectorList underMin;

        // Copying a value just needs to copy our two vectors.
        void copy(const CLxValue* from) override;

        // Compare mostly just needs return a non-zero if the values are
        // different. In theory these can be sorted too, but that doesn't
        // make a lot of sense for our two vectors.
        int compare(const CLxValue* from) override;
    };

    void Value::copy(const CLxValue* from)
    {
        const Value* v = dynamic_cast<const Value*>(from);

        overMax  = v->overMax;
        underMin = v->underMin;
    }

    int Value::compare(const CLxValue* from)
    {
        const Value* v = dynamic_cast<const Value*>(from);

        if (overMax != v->overMax)
            return -1;
        else if (underMin != v->underMin)
            return 1;

        return 0;
    }

    // Create a static CLxMeta_Value with our class and add it to a root meta object,
    // which will handle all the com register cruft when modo loads this plugin.
    // We register this separately from the item just to be sure the value type is registered
    // before the item that uses it, as that would cause a failure in plugin loading, and
    // those are really hard to debug from the outside.
    static CLxMeta_Value<Value> val_meta(servers::value.c_str());
    static class CRoot : public CLxMetaRoot
    {
        bool pre_init() override
        {
            add(&val_meta);
            return false;
        }
    } root_meta;
}  // namespace polyListData

// The more interesting side of things is our item type itself.
namespace thicknessMeasurer
{
    // The CLxChannels class defines the channels on our item.
    // We just have our min and max thicknesses, and our custom
    // channel type holding the currently too thick/thin polys.
    class Channels : public CLxChannels
    {
    public:
        void init_chan(CLxAttributeDesc& desc) override
        {
            desc.add(channels::max.c_str(), LXsTYPE_DISTANCE);
            desc.default_val(0.0);

            desc.add(channels::min.c_str(), LXsTYPE_DISTANCE);
            desc.default_val(0.0);

            desc.add(channels::polylist.c_str(), polyListData::val_meta.type_name());
            desc.set_storage();
        }
    };

    // The DotDrawer draws the dots...  It reads our custom data from the item as a generic value, then
    // casts it to our established polyListData::value type to read its member data.
    class DotDrawer : public CLxViewItem3D
    {
        void draw(CLxUser_Item& item, CLxUser_ChannelRead& chan, CLxUser_StrokeDraw& stroke, int, const CLxVector&) override
        {
            static const LXtVector red{ 1.0, 0.0, 0.0 };
            static const LXtVector blue{ 0.0, 0.0, 1.0 };

            CLxUser_Value val;

            if (chan.Object(item, channels::polylist.c_str(), val))
            {
                auto* pListWrap = polyListData::val_meta.cast(val);
                if (!pListWrap->overMax.empty())
                {
                    stroke.BeginPoints(3.0, red, 1.0);
                    for (auto& pt : pListWrap->overMax)
                        stroke.Vertex3(pt[0], pt[1], pt[2], 0);
                }

                if (!pListWrap->underMin.empty())
                {
                    stroke.BeginPoints(3.0, blue, 1.0);
                    for (auto& pt : pListWrap->underMin)
                        stroke.Vertex3(pt[0], pt[1], pt[2], 0);
                }
            }
        }
    };

    // The modifier reads the user's min/max channels, evaluates the linked mesh, and then writes out the
    // custom channel data for the list of polys which are outside of the min/max range.
    class Modifier : public CLxEvalModifier
    {
        void bind(CLxUser_Item& item, unsigned ident) override;
        bool change_test() override;
        void eval() override;

    private:
        bool m_valid{};

        void writeThicknessValue(const double max, const double min, CLxUser_Mesh& mesh, polyListData::Value* val);
    };

    void Modifier::bind(CLxUser_Item& item, unsigned ident)
    {
        mod_add_chan(item, channels::polylist.c_str(), LXfECHAN_WRITE);
        mod_add_chan(item, channels::max.c_str(), LXfECHAN_READ);
        mod_add_chan(item, channels::min.c_str(), LXfECHAN_READ);

        CLxUser_Scene     scene(item);
        CLxUser_ItemGraph itemGraph;
        CLxUser_Item      linkedMesh;

        scene.GraphLookup(servers::graph.c_str(), itemGraph);
        if (itemGraph.test() && itemGraph.Reverse(item, 0, linkedMesh))
        {
            mod_add_chan(linkedMesh, LXsICHAN_MESH_MESH, LXfECHAN_READ);
            m_valid = true;
        }
    }

    bool Modifier::change_test()
    {
        return false;
    }

    void Modifier::eval()
    {
        if (!m_valid)
            return;

        auto* attr = mod_attr();

        CLxUser_Value valObj;
        attr->ObjectRW(0, valObj);

        CLxUser_MeshFilter mFilt;
        attr->ObjectRO(3, mFilt);

        if (!mFilt.test() || !valObj.test())
            return;

        auto maxVal = attr->Float(1);
        auto minVal = attr->Float(2);

        CLxUser_Mesh mesh;
        mFilt.GetMesh(mesh);

        auto* val = polyListData::val_meta.cast(valObj);
        if (val && mesh.test())
            writeThicknessValue(maxVal, minVal, mesh, val);
    }

    void Modifier::writeThicknessValue(const double max, const double min, CLxUser_Mesh& mesh, polyListData::Value* val)
    {
        val->overMax.clear();
        val->underMin.clear();

        CLxUser_Polygon polys(mesh);
        const auto      polyCount = mesh.NPolygons();
        for (auto i = 0; i < polyCount; ++i)
        {
            LXtVector pos;
            LXtVector norm;

            polys.SelectByIndex(i);
            polys.RepresentativePosition(pos);
            polys.Normal(norm);
            LXx_VSCL(norm, -1.0);

            LXtVector hitNorm;
            double    hitDist;
            bool      hitValid = polys.IntersectRay(pos, norm, hitNorm, &hitDist) == LXe_TRUE ? true : false;
            if (!hitValid)
                continue;

            if (hitDist > max)
                val->overMax.push_back({ pos[0], pos[1], pos[2] });
            else if (hitDist < min)
                val->underMin.push_back({ pos[0], pos[1], pos[2] });
        }
    }

    // The metaclass registration is confusing, but allows for a lot less code than the
    // older com interface stuff (which is also confusing, to be fair).  We build a hierarchy
    // of meta servers for the root meta object to consume and work out which servers link to
    // what.  Drawing meta objects must be added directly to the package meta server they draw,
    // but otherwise the root meta object and a few customization calls will put it all together
    // for us.
    static CLxMeta_Channels<Channels>    chan_meta;
    static CLxMeta_Package<CLxPackage>   pkg_meta(servers::package.c_str());
    static CLxMeta_ViewItem3D<DotDrawer> v3d_meta;

    static CLxMeta_SchematicConnection<CLxSchematicConnection> schm_meta(servers::graph.c_str());
    static CLxMeta_EvalModifier<Modifier>                      mod_meta(servers::modifier.c_str());

    static class CRoot : public CLxMetaRoot
    {
        bool pre_init() override
        {
            pkg_meta.set_supertype(LXsITYPE_ITEMMODIFY);
            pkg_meta.add_tag(LXsPKG_GRAPHS, servers::graph.c_str());
            pkg_meta.add(&v3d_meta);

            schm_meta.set_itemtype(servers::package.c_str());
            schm_meta.set_graph(servers::graph.c_str());

            mod_meta.add_dependent_graph(servers::graph.c_str());

            add(&chan_meta);
            add(&schm_meta);
            add(&pkg_meta);
            add(&mod_meta);

            return false;
        }
    } root_meta;

};  // namespace thicknessMeasurer
