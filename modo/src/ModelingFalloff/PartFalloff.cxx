#include "PartFalloff.hxx"

#include <lxsdk/lx_draw.hpp>
#include <lxsdk/lx_handles.hpp>
#include <lxsdk/lx_layer.hpp>

#include <array>
#include <cassert>
#include <unordered_map>
#include <vector>

namespace global
{
    namespace id
    {
        static std::string const tool{ "part.falloff" };
        static std::string const packet{ "part.falloff.packet" };

        static constexpr int startPt = 0x01000;
        static constexpr int endPt   = 0x01001;
        static constexpr int steps   = LXiHITPART_INVIS;
    }  // namespace id

    namespace attrs
    {
        static std::string const mode{ "mode" };
        static std::string const startX{ "start.x" };
        static std::string const startY{ "start.y" };
        static std::string const startZ{ "start.z" };
        static std::string const endX{ "end.x" };
        static std::string const endY{ "end.y" };
        static std::string const endZ{ "end.z" };
        static std::string const seed{ "seed" };
        static std::string const scale{ "scale" };

        static std::vector<std::pair<std::string, std::string>> typeMap{
            { mode, LXsTYPE_INTEGER },    { startX, LXsTYPE_DISTANCE }, { startY, LXsTYPE_DISTANCE },
            { startZ, LXsTYPE_DISTANCE }, { endX, LXsTYPE_DISTANCE },   { endY, LXsTYPE_DISTANCE },
            { endZ, LXsTYPE_DISTANCE },   { scale, LXsTYPE_PERCENT },   { seed, LXsTYPE_INTEGER },
        };
    }  // namespace attrs

    LXtTextValueHint falloffModes[] = { { static_cast<uint32_t>(FalloffMode::Position), "Position" },
                                        { static_cast<uint32_t>(FalloffMode::Random), "Random" },
                                        { -1, NULL } };

}  // namespace global

namespace com
{
    namespace init
    {
        static void tool()
        {
            CLxGenericPolymorph* srv = new CLxPolymorph<partFalloff::Tool>;
            srv->AddInterface(new CLxIfc_Tool<partFalloff::Tool>);
            srv->AddInterface(new CLxIfc_ToolModel<partFalloff::Tool>);
            srv->AddInterface(new CLxIfc_Attributes<partFalloff::Tool>);
            srv->AddInterface(new CLxIfc_StaticDesc<partFalloff::Tool>);
            lx::AddServer(global::id::tool.c_str(), srv);
        }

        static void packet()
        {
            CLxGenericPolymorph* srv = new CLxPolymorph<partFalloff::Packet>;
            srv->AddInterface(new CLxIfc_FalloffPacket<partFalloff::Packet>);
            lx::AddSpawner(global::id::packet.c_str(), srv);
        }
    }  // namespace init

    namespace spawn
    {
        static partFalloff::Packet* packet(void** ppvObj)
        {
            static CLxSpawner<partFalloff::Packet> spawner(global::id::packet.c_str());
            return spawner.Alloc(ppvObj);
        }
    }  // namespace spawn
}  // namespace com

namespace component
{
    void Attributes::initAttrs(const AttributeTypeMap& attrList)
    {
        uint32_t idx     = 0;
        auto     addAttr = [&](const AttributeDefinition& attr)
        {
            dyna_Add(attr.first, attr.second);
            m_attrMap[attr.first] = idx++;
        };

        for (const auto& attr : attrList)
            addAttr(attr);
    }

    uint32_t Attributes::index(const std::string& attr)
    {
        assert(m_attrMap.find(attr) != m_attrMap.end());
        return m_attrMap.at(attr);
    }

    void Attributes::setHint(const std::string& attr, const LXtTextValueHint* hint)
    {
        assert(m_attrMap.find(attr) != m_attrMap.end());

        dyna_SetHint(m_attrMap.at(attr), hint);
    }

    template <class T>
    T Attributes::getAttr(const std::string& attr)
    {
        assert(m_attrMap.find(attr) != m_attrMap.end());

        T val{};
        if constexpr (std::is_same_v<T, double>)
            val = dyna_Float(m_attrMap.at(attr));
        else if constexpr (std::is_same_v<T, int>)
            val = dyna_Int(m_attrMap.at(attr));

        return val;
    }

    template <>
    std::string Attributes::getAttr<std::string>(const std::string& attr)
    {
        assert(m_attrMap.find(attr) != m_attrMap.end());

        std::string val;
        dyna_String(m_attrMap.at(attr), val);
        return val;
    }

    template <>
    CLxVector Attributes::getAttr<CLxVector>(const std::string& attr)
    {
        assert(m_attrMap.find(attr) != m_attrMap.end());

        CLxVector val;
        uint32_t  start = m_attrMap.at(attr);
        for (auto i = 0u; i < 3u; ++i)
            val[i] = dyna_Float(start + i);

        return val;
    }

    void PartMap::buildFromMesh(CLxUser_Mesh& mesh)
    {
        std::unordered_map<uint32_t, CLxPositionData> boxes;

        CLxUser_Point pointAcc;
        pointAcc.fromMesh(mesh);

        const auto nPts = mesh.NPoints();
        for (auto i = 0; i < nPts; ++i)
        {
            pointAcc.SelectByIndex(i);

            uint32_t part;
            pointAcc.Part(&part);

            LXtFVector v;
            pointAcc.Pos(v);

            CLxPositionData& data = boxes[part];
            data.add(v);
        }

        CLxBoundingBox boundary{};
        for (auto& [part, box] : boxes)
        {
            m_map.emplace(std::piecewise_construct, std::make_tuple(part), std::make_tuple(box.center(), box.axis()));
            boundary.add(box.center());
        }
        LXx_VCPY(m_min.v, boundary._min);
        LXx_VCPY(m_max.v, boundary._max);
    }

    const std::pair<CLxVector, CLxVector> PartMap::bounds() const
    {
        return std::make_pair(m_min, m_max);
    }

    bool PartMap::empty() const
    {
        return m_map.empty();
    }

    const global::MeshPartData* PartMap::get(uint32_t part) const
    {
        auto it = m_map.find(part);
        return it != m_map.end() ? &it->second : nullptr;
    }

    std::optional<double> Cache::get(uint32_t part)
    {
        auto it = m_weights.find(part);
        return it != m_weights.end() ? it->second : std::optional<double>{};
    }

    void Cache::set(uint32_t part, double weight)
    {
        m_weights[part] = weight;
    }

    void Cache::clear()
    {
        m_weights.clear();
    }

}  // namespace component

// Declarations
namespace partFalloff
{
    // Modeling falloffs are tools added to the tool pipe which populate the falloff packet.
    // Other downstream tools (either in the toolpipe or meshops with a link to the toolop)
    // can then access them and query falloff strengths.
    Tool::Tool()
    {
        // Create a vectortype for our tool and store the offset where we'll
        // inject the falloff packet we create.
        CLxUser_PacketService pktSvc;
        pktSvc.NewVectorType(LXsCATEGORY_TOOL, m_vType);

        pktSvc.AddPacket(m_vType, LXsP_TOOL_FALLOFF, LXfVT_SET);
        m_pktOffset = pktSvc.GetOffset(LXsCATEGORY_TOOL, LXsP_TOOL_FALLOFF);

        pktSvc.AddPacket(m_vType, LXsP_TOOL_EVENTTRANS, LXfVT_GET);
        m_handlesOffset = pktSvc.GetOffset(LXsCATEGORY_TOOL, LXsP_TOOL_EVENTTRANS);

        pktSvc.AddPacket(m_vType, LXsP_TOOL_INPUT_EVENT, LXfVT_GET);
        m_inputOffset = pktSvc.GetOffset(LXsCATEGORY_TOOL, LXsP_TOOL_INPUT_EVENT);

        pktSvc.AddPacket(m_vType, LXsP_TOOL_ACTCENTER, LXfVT_GET);
        m_actionOffset = pktSvc.GetOffset(LXsCATEGORY_TOOL, LXsP_TOOL_ACTCENTER);

        initAttrs(global::attrs::typeMap);
        setHint(global::attrs::mode, global::falloffModes);
    }

    LXtObjectID Tool::tool_VectorType()
    {
        return m_vType.m_loc;
    }

    const char* Tool::tool_Order()
    {
        return LXs_ORD_WGHT;
    }

    LXtID4 Tool::tool_Task()
    {
        return LXi_TASK_WGHT;
    }

    void Tool::validatePkt()
    {
        CLxUser_LayerService lsrv;
        CLxUser_LayerScan    scan;
        CLxUser_Mesh         mesh;

        lsrv.BeginScan(LXf_LAYERSCAN_PRIMARY, scan);
        if (!scan.BaseMeshByIndex(0, mesh) || !mesh.test())
            return;

        if (!m_primaryMesh.test() || !m_primaryMesh.IsSame(mesh))
        {
            m_falloffPkt = nullptr;
            m_pktComPtr  = nullptr;
            m_primaryMesh.set(mesh);
        }

        if (!m_falloffPkt)
        {
            m_falloffPkt = com::spawn::packet(&m_pktComPtr);
            m_falloffPkt->setupMesh(m_primaryMesh);
        }
    }

    void Tool::tool_Evaluate(ILxUnknownID vts)
    {
        validatePkt();

        m_falloffPkt->update(*this);

        CLxUser_VectorStack vecStack(vts);
        vecStack.SetPacket(m_pktOffset, m_pktComPtr);
    }

    uint32_t Tool::tmod_Flags()
    {
        return LXfTMOD_DRAW_3D | LXfTMOD_I0_INPUT;
    }

    static void drawSteps(ILxUnknownID drawIFC, const CLxVector& start, const CLxVector& end)
    {
        static LXtVector STEP_COLOR{ 0.8, 0.6, 1.0 };
        static double    PIXEL_WIDTH = 50.0;
        static uint32_t  STEP_COUNT  = 4u;

        CLxUser_StrokeDraw stroke(drawIFC);
        CLxUser_View       view(drawIFC);
        assert(stroke.test() && view.test());

        // We want to draw steps that start at the end and go down to the start..
        // We need to know the direction to offset the step height, which is the
        // cross between the start-end vector and the view's eye vector.
        CLxVector delta = end - start;
        double    dLen  = delta.length();
        CLxVector midPt = (end + start) / 2.0;
        CLxVector crossVec;
        view.EyeVector(midPt.v, crossVec.v);

        crossVec.normalize();
        delta.normalize();
        auto upVec = crossVec.cross(delta);
        upVec.normalize();

        double height3d   = PIXEL_WIDTH * view.PixelScale();
        double stepWidth  = dLen / (STEP_COUNT);
        double stepHeight = height3d / (STEP_COUNT);

        stroke.SetPart(global::id::steps);
        stroke.Begin(LXiSTROKE_LINE_LOOP, STEP_COLOR, 1.0);
        stroke.Vert(const_cast<double*>(end.v));

        CLxVector fullOffset = upVec * height3d;
        stroke.Vert(fullOffset, LXiSTROKE_RELATIVE);

        CLxVector stepRun  = delta * stepWidth;
        CLxVector stepRise = upVec * stepHeight;
        CLxVector runInv   = stepRun * -1.0;
        CLxVector riseInv  = stepRise * -1.0;
        for (auto i = 0; i < STEP_COUNT; ++i)
        {
            stroke.Vert(runInv, LXiSTROKE_RELATIVE);
            stroke.Vert(riseInv, LXiSTROKE_RELATIVE);
        }

        stroke.Vert(riseInv, LXiSTROKE_RELATIVE);
        for (auto i = 0; i < STEP_COUNT; ++i)
        {
            stroke.Vert(stepRun, LXiSTROKE_RELATIVE);
            if (i != (STEP_COUNT - 1))
                stroke.Vert(riseInv, LXiSTROKE_RELATIVE);
        }
    }

    void Tool::tmod_Draw(ILxUnknownID, ILxUnknownID stroke, int)
    {
        if (getAttr<int>(global::attrs::mode) != static_cast<int>(global::FalloffMode::Position))
            return;

        CLxUser_HandleDraw draw(stroke);

        auto start = getAttr<CLxVector>(global::attrs::startX);
        draw.Handle(start.v, nullptr, global::id::startPt, 0);

        auto end = getAttr<CLxVector>(global::attrs::endX);
        draw.Handle(end.v, nullptr, global::id::endPt, 0);

        drawSteps(stroke, start, end);
    }

    void Tool::tmod_Test(ILxUnknownID vts, ILxUnknownID stroke, int flags)
    {
        tmod_Draw(vts, stroke, flags);
    }

    void Tool::setHandles(ILxUnknownID adjust, const CLxVector& pos, uint32_t firstIdx)
    {
        CLxUser_AdjustTool at(adjust);
        assert(at.test());

        for (auto i = 0u; i < 3u; ++i)
            at.SetFlt(firstIdx + i, pos[i]);
    }

    void Tool::tmod_Initialize(ILxUnknownID vts, ILxUnknownID adjust, unsigned int flags)
    {
        validatePkt();
        if (!m_isSetup)
        {
            m_isSetup = true;
            CLxUser_AdjustTool at(adjust);
            at.SetFlt(index(global::attrs::scale), 1.0);
            auto& bounds = m_falloffPkt->partBounds();
            setHandles(adjust, bounds.first, index(global::attrs::startX));
            setHandles(adjust, bounds.second, index(global::attrs::endX));
        }
    }

    LxResult Tool::tmod_Down(ILxUnknownID vts, ILxUnknownID adjust)
    {
        CLxUser_VectorStack          vec(vts);
        CLxUser_EventTranslatePacket eventData;

        auto* inputData = static_cast<LXpToolInputEvent*>(vec.Read(m_inputOffset));
        vec.ReadObject(m_handlesOffset, eventData);

        // On the first mouse down, we setup the tool handles.
        if (!m_isSetup)
        {
            m_isSetup = true;
            m_inReset = true;
            auto end  = getAttr<CLxVector>(global::attrs::endX);
            eventData.HitHandle(vts, end);
            inputData->part = global::id::endPt;
        }
        else if (inputData->part == global::id::startPt || inputData->part == global::id::endPt)
        {
            auto& attr   = inputData->part == global::id::startPt ? global::attrs::startX : global::attrs::endX;
            auto  hitPos = getAttr<CLxVector>(attr);
            eventData.HitHandle(vec, hitPos.v);
        }
        else
        {
            auto* acenData = static_cast<LXpToolActionCenter*>(vec.Read(m_actionOffset));

            setHandles(adjust, acenData->v, index(global::attrs::startX));
            setHandles(adjust, acenData->v, index(global::attrs::endX));
            eventData.HitHandle(vts, acenData->v);
            inputData->part = global::id::endPt;
            m_inReset       = true;
        }
        return LXe_OK;
    }

    void Tool::tmod_Move(ILxUnknownID vts, ILxUnknownID adjust)
    {
        CLxUser_VectorStack vec(vts);
        auto*               inputData = static_cast<LXpToolInputEvent*>(vec.Read(m_inputOffset));
        if (m_inReset || inputData->part == global::id::startPt || inputData->part == global::id::endPt)
        {
            CLxUser_AdjustTool           at(adjust);
            CLxUser_EventTranslatePacket eventData;
            vec.ReadObject(m_handlesOffset, eventData);

            CLxVector dragPos;
            eventData.GetNewPosition(vec, dragPos);
            uint32_t firstIdx = inputData->part == global::id::startPt ? index(global::attrs::startX) : index(global::attrs::endX);
            for (auto i = 0u; i < 3; ++i)
                at.SetFlt(firstIdx + i, dragPos[i]);
        }
    }

    void Tool::tmod_Up(ILxUnknownID vts, ILxUnknownID adjust)
    {
        m_inReset = false;
    }

    LXtTagInfoDesc Tool::descInfo[] = {
        { LXsSRV_USERNAME, "tool.part.falloff" },
    };

    void Packet::setupMesh(CLxUser_Mesh& mesh)
    {
        m_pointAcc.fromMesh(mesh);
        m_partData.buildFromMesh(mesh);
    }

    const std::pair<CLxVector, CLxVector> Packet::partBounds() const
    {
        return m_partData.bounds();
    }

    // Populates or updates the tool settings struct
    void Packet::update(Tool& tool)
    {
        global::ToolSettings tmpSettings{};

        tmpSettings.minPos = tool.getAttr<CLxVector>(global::attrs::startX);
        tmpSettings.maxPos = tool.getAttr<CLxVector>(global::attrs::endX);
        tmpSettings.mode   = static_cast<global::FalloffMode>(tool.getAttr<int>(global::attrs::mode));
        tmpSettings.scale  = tool.getAttr<double>(global::attrs::scale);
        tmpSettings.seed   = tool.getAttr<int>(global::attrs::seed);

        if (m_settings != tmpSettings)
            m_weightCache.clear();

        m_settings = tmpSettings;
    }

    double Packet::fp_Evaluate(LXtFVector, LXtPointID vrx, LXtPolygonID)
    {
        if (!vrx || !m_pointAcc.test() || m_partData.empty())
            return 1.0;

        uint32_t currentPart{};
        m_pointAcc.Select(vrx);
        m_pointAcc.Part(&currentPart);

        auto val = m_weightCache.get(currentPart);
        if (val)
            return m_settings.scale * val.value();

        auto cacheAndReturn = [&](double w)
        {
            m_weightCache.set(currentPart, w);
            return w * m_settings.scale;
        };

        CLxEaseFraction   remap;
        CLxPerlin<double> noise(4, 1, 1, m_settings.seed);
        CLxVector         v;

        auto* part = m_partData.get(currentPart);
        assert(part);

        switch (m_settings.mode)
        {
            case global::FalloffMode::Random:
                return cacheAndReturn(noise.eval(part->center));

            case global::FalloffMode::Position:
                v = m_settings.maxPos - m_settings.minPos;
                remap.set_shape(LXiESHP_LINEAR);

                auto offsetVec = part->center - m_settings.minPos;
                auto num       = offsetVec.dot(v);
                auto den       = v.lengthSquared();
                if (den)
                    return cacheAndReturn(remap.evaluate(num / den));
        }

        return cacheAndReturn(1.0);
    }

}  // namespace partFalloff

void initialize()
{
    com::init::tool();
    com::init::packet();
}
