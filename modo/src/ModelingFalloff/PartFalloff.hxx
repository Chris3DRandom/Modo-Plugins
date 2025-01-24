#pragma once

#include <lxsdk/ex_toolPacketWrap.hpp>

#include <lxsdk/lx_tool.hpp>
#include <lxsdk/lx_toolui.hpp>
#include <lxsdk/lx_vector.hpp>
#include <lxsdk/lx_vmodel.hpp>
#include <lxsdk/lx_vp.hpp>
#include <lxsdk/lxidef.h>
#include <lxsdk/lxu_attrdesc.hpp>
#include <lxsdk/lxu_math.hpp>
#include <lxsdk/lxu_vector.hpp>

#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>

// The part falloff is a tool that applies the same falloff percentage to all polys/edges/verts
// that are part of the same mesh island.
namespace global
{
    // Weight values are assigned to parts in one of 2 ways:
    //  - Part position, similar usage to the linear falloff tool
    //  - Randomly
    enum class FalloffMode : uint32_t
    {
        Position,
        Random
    };

    extern LXtTextValueHint falloffModes[];

    struct ToolSettings
    {
        global::FalloffMode mode;
        CLxVector           minPos;
        CLxVector           maxPos;
        CLxVector           viewVector;
        int                 seed;
        double              scale;

        bool operator==(const ToolSettings& other) const
        {
            // Deliberatly ignore scale, we just always apply that on top of cached values.
            return (seed == other.seed && mode == other.mode && minPos == other.minPos && maxPos == other.maxPos && viewVector == other.viewVector);
        }

        bool operator!=(const ToolSettings& other) const
        {
            return !(*this == other);
        }
    };

    struct MeshPartData
    {
        MeshPartData(CLxVector& c, CLxVector& v) : center(c), vector(v)
        {
        }

        CLxVector center{ 0.0, 0.0, 0.0 };
        CLxVector vector{ 0.0, 0.0, 0.0 };
    };
}  // namespace global

namespace component
{
    // Attributes are the tool's properties.
    class Attributes : public CLxDynamicAttributes
    {
    public:
        using AttributeDefinition = std::pair<std::string, std::string>;
        using AttributeTypeMap    = std::vector<std::pair<std::string, std::string>>;
        using AttributeOffsetMap  = std::unordered_map<std::string, uint32_t>;

        void initAttrs(const AttributeTypeMap& attrList);

        void     setHint(const std::string& attr, const LXtTextValueHint* hint);
        uint32_t index(const std::string& attr);

        template <typename T>
        T getAttr(const std::string& attr);
        template <>
        std::string getAttr<std::string>(const std::string& attr);
        template <>
        CLxVector getAttr<CLxVector>(const std::string& attr);

    private:
        AttributeOffsetMap   m_attrMap;
        CLxUser_ValueService m_valSvc;
    };

    class PartMap
    {
    public:
        void buildFromMesh(CLxUser_Mesh& mesh);

        bool empty() const;

        const std::pair<CLxVector, CLxVector> bounds() const;

        const global::MeshPartData* get(uint32_t part) const;

    private:
        std::unordered_map<uint32_t, global::MeshPartData> m_map;
        CLxVector                                          m_min;
        CLxVector                                          m_max;
    };

    class Cache
    {
    public:
        std::optional<double> get(uint32_t part);
        void                  set(uint32_t part, double weight);
        void                  clear();

    private:
        std::unordered_map<uint32_t, double> m_weights;
    };
}  // namespace component

namespace partFalloff
{
    class Packet;

    // The tool is responsible for user interaction, reading and setting tool attributes,
    // drawing handles, and ultimately creating/updating the tool operation (ToolOp) object.
    class Tool : public CLxImpl_Tool, public CLxImpl_ToolModel, public component::Attributes
    {
    public:
        Tool();

        LXtObjectID tool_VectorType() override;
        const char* tool_Order() override;
        LXtID4      tool_Task() override;
        void        tool_Evaluate(ILxUnknownID vts) override;

        void     tmod_Initialize(ILxUnknownID vts, ILxUnknownID adjust, unsigned int flags) override;
        uint32_t tmod_Flags() override;
        void     tmod_Draw(ILxUnknownID vts, ILxUnknownID stroke, int flags) override;
        void     tmod_Test(ILxUnknownID vts, ILxUnknownID stroke, int flags) override;
        LxResult tmod_Down(ILxUnknownID vts, ILxUnknownID adjust) override;
        void     tmod_Move(ILxUnknownID vts, ILxUnknownID adjust) override;
        void     tmod_Up(ILxUnknownID vts, ILxUnknownID adjust) override;

        static LXtTagInfoDesc descInfo[];

    private:
        CLxUser_Mesh m_primaryMesh;

        void validatePkt();
        void setHandles(ILxUnknownID adjust, const CLxVector& pos, uint32_t firstIdx);

        bool m_isSetup{};
        bool m_inReset{};

        CLxUser_VectorType m_vType;
        uint32_t           m_pktOffset{};
        uint32_t           m_handlesOffset{};
        uint32_t           m_inputOffset{};
        uint32_t           m_actionOffset{};

        Packet* m_falloffPkt{};
        void*   m_pktComPtr{};
    };

    class Packet : public CLxImpl_FalloffPacket
    {
    public:
        void setupMesh(CLxUser_Mesh& mesh);
        void update(Tool& tool);

        const std::pair<CLxVector, CLxVector> partBounds() const;

        double fp_Evaluate(LXtFVector, LXtPointID vrx, LXtPolygonID) override;

    private:
        CLxUser_Point m_pointAcc;

        component::PartMap   m_partData;
        component::Cache     m_weightCache;
        global::ToolSettings m_settings;
    };

}  // namespace partFalloff

// Plugin Registration called by modo on startup
void initialize();
