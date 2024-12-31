#include <lxsdk/ex_toolPacketWrap.hpp>
#include <lxsdk/lx_action.hpp>
#include <lxsdk/lx_item.hpp>
#include <lxsdk/lx_layer.hpp>
#include <lxsdk/lx_locator.hpp>
#include <lxsdk/lx_log.hpp>
#include <lxsdk/lx_package.hpp>
#include <lxsdk/lx_schematic.hpp>
#include <lxsdk/lx_tool.hpp>
#include <lxsdk/lx_vector.hpp>
#include <lxsdk/lxidef.h>
#include <lxsdk/lxu_math.hpp>
#include <lxsdk/lxu_matrix.hpp>
#include <lxsdk/lxu_modifier.hpp>

#include <array>
#include <unordered_map>
#include <vector>

// Declarations
namespace partFalloff
{
    // Modeling falloffs are tools added to the tool pipe which populate the falloff packet.
    // Other downstream tools (either in the toolpipe or meshops with a link to the toolop)
    // can then access them and query falloff strengths.
    namespace idents
    {
        static std::string const tool{ "part.falloff.tool" };
        static std::string const packet{ "part.falloff.packet" };

        static std::string const attrAxis{ "axis" };
        static std::string const attrInvert{ "invert" };
    }  // namespace idents

    namespace tool
    {
        struct MeshPartData
        {
            LXtFVector center{ 0.0, 0.0, 0.0 };
            double     tmpWeight{ -1.0 };
        };

        class PartFalloff : public CLxImpl_FalloffPacket, public CLxVisitor
        {
        public:
            enum class FalloffAxis : uint32_t
            {
                X,
                Y,
                Z,
                Random
            };

            void buildPartMap(CLxUser_Mesh& mesh)
            {
                std::unordered_map<uint32_t, CLxBoundingBox> boxes;
                m_pointAcc.fromMesh(mesh);
                for (auto i = 0; i < mesh.NPoints(); ++i)
                {
                    m_pointAcc.SelectByIndex(i);

                    uint32_t part;
                    m_pointAcc.Part(&part);
                    if (boxes.find(part) == boxes.end())
                        boxes[part] = CLxBoundingBox();

                    LXtFVector v;
                    m_pointAcc.Pos(v);

                    boxes.at(part).add(v);
                }

                m_partMap.clear();
                for (auto const& [part, box] : boxes)
                {
                    m_partMap[part] = MeshPartData();
                    LXx_VCPY(m_partMap.at(part).center, box.center());
                }
            }

            void setAxis(FalloffAxis axis)
            {
                m_axis = axis;
                for (auto& [id, part] : m_partMap)
                    part.tmpWeight = -1.0;
            }

            static PartFalloff* spawn(void** ppvObj)
            {
                static CLxSpawner<PartFalloff> spawner(idents::packet.c_str());
                return spawner.Alloc(ppvObj);
            }

            double fp_Evaluate(LXtFVector, LXtPointID vrx, LXtPolygonID) override
            {
                if (!m_pointAcc.test() || m_partMap.empty())
                    return 1.0;

                uint32_t currentPart{};
                m_pointAcc.Select(vrx);
                m_pointAcc.Part(&currentPart);

                auto it = m_partMap.find(currentPart);
                if (it == m_partMap.end())
                {
                    // Something's gone horribly wrong!
                    return 1.0;
                }

                if (it->second.tmpWeight >= 0.0)
                    return it->second.tmpWeight;

                if (m_axis == FalloffAxis::Random)
                {
                    CLxPerlin<double> noise;
                    for (auto& [id, part] : m_partMap)
                        part.tmpWeight = noise.eval(part.center);
                }
                else
                {
                    double   min(std::numeric_limits<double>::max());
                    double   max(std::numeric_limits<double>::min());
                    uint32_t axis = static_cast<uint32_t>(m_axis);
                    for (auto& [id, part] : m_partMap)
                    {
                        if (part.center[axis] < min)
                            min = part.center[axis];
                        else if (part.center[axis] > max)
                            max = part.center[axis];
                    }

                    CLxEaseFraction remap;
                    double          range = max - min;
                    remap.set_shape(LXiESHP_LINEAR);
                    for (auto& [id, part] : m_partMap)
                    {
                        double pct     = (part.center[axis] - min) / range;
                        part.tmpWeight = remap.evaluate(pct);
                    }
                }

                return it->second.tmpWeight;
            }

        private:
            CLxUser_Point                              m_pointAcc;
            FalloffAxis                                m_axis;
            std::unordered_map<uint32_t, MeshPartData> m_partMap;
        };

        // The tool is responsible for spawning the Falloff packet object and
        // setting it in the toolpipe.  This same pattern is used for setting
        // pretty much any tool packet in the tool pipe, including custom ones.
        class FalloffTool : public CLxImpl_Tool, public CLxDynamicAttributes
        {
        public:
            FalloffTool()
            {
                CLxUser_PacketService pktSvc;
                pktSvc.NewVectorType(LXsCATEGORY_TOOL, m_vType);

                pktSvc.AddPacket(m_vType, LXsP_TOOL_FALLOFF, LXfVT_SET);
                m_fallOff = pktSvc.GetOffset(LXsCATEGORY_TOOL, LXsP_TOOL_FALLOFF);

                pktSvc.AddPacket(m_vType, LXsP_TOOL_SUBJECT, LXfVT_GET);
                m_subOff = pktSvc.GetOffset(LXsCATEGORY_TOOL, LXsP_TOOL_SUBJECT);

                dyna_Add(idents::attrAxis, LXsTYPE_AXIS);
                dyna_Add(idents::attrInvert, LXsTYPE_BOOLEAN);

                // To do - create a custom gradient value type since the modo ones aren't
                // exposed and aren't very good anyway!
            }

            LXtObjectID tool_VectorType() override
            {
                return m_vType.m_loc;
            }

            const char* tool_Order() override
            {
                return LXs_ORD_WGHT;
            }

            LXtID4 tool_Task() override
            {
                return LXi_TASK_WGHT;
            }

            void tool_Evaluate(ILxUnknownID vts) override
            {
                CLxUser_VectorStack vecStack(vts);

                void* comPtr{};
                auto* pkt = PartFalloff::spawn(&comPtr);
                vecStack.SetPacket(m_fallOff, comPtr);

                int axis;
                attr_GetInt(0, &axis);
                pkt->setAxis(static_cast<PartFalloff::FalloffAxis>(axis));

                CLxUser_LayerService lsrv;
                CLxUser_LayerScan    scan;
                CLxUser_Mesh         mesh;

                lsrv.BeginScan(LXf_LAYERSCAN_PRIMARY, scan);
                if (scan.BaseMeshByIndex(0, mesh))
                    pkt->buildPartMap(mesh);
            }

        private:
            CLxUser_VectorType m_vType;
            uint32_t           m_subOff{};
            uint32_t           m_fallOff{};
        };
    }  // namespace tool

}  // namespace partFalloff

void initialize()
{
    CLxGenericPolymorph* srv = new CLxPolymorph<partFalloff::tool::FalloffTool>;
    srv->AddInterface(new CLxIfc_Tool<partFalloff::tool::FalloffTool>);
    srv->AddInterface(new CLxIfc_Attributes<partFalloff::tool::FalloffTool>);
    lx::AddServer(partFalloff::idents::tool.c_str(), srv);

    CLxGenericPolymorph* pktSrv = new CLxPolymorph<partFalloff::tool::PartFalloff>;
    pktSrv->AddInterface(new CLxIfc_FalloffPacket<partFalloff::tool::PartFalloff>);
    lx::AddSpawner(partFalloff::idents::packet.c_str(), pktSrv);
}
