#include "PartFalloff.hxx"

#include <lxsdk/lx_draw.hpp>
#include <lxsdk/lx_handles.hpp>
#include <lxsdk/lx_layer.hpp>
#include <lxsdk/lx_log.hpp>

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
        static std::string const package{ "part.falloff.item" };
        static std::string const instance{ "part.falloff.inst" };
        static std::string const falloff{ "part.falloff.falloff" };
        static std::string const modifier{ "part.falloff.mod" };

        static constexpr int startPt = 0x01000;
        static constexpr int endPt   = 0x01001;
        static constexpr int steps   = LXiHITPART_INVIS;
    }  // namespace id

    namespace drawing
    {
        static LXtVector ToolColor{ 0.8, 0.6, 1.0 };
        static double    PixelWidth = 50.0;
        static uint32_t  Steps      = 4u;
    }  // namespace drawing

    namespace attrs
    {
        static std::string const mode{ "mode" };
        static std::string const start{ "start" };
        static std::string const startX{ "start.X" };
        static std::string const startY{ "start.Y" };
        static std::string const startZ{ "start.Z" };
        static std::string const end{ "end" };
        static std::string const endX{ "end.X" };
        static std::string const endY{ "end.Y" };
        static std::string const endZ{ "end.Z" };
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
    template <typename T>
    static T evalFalloff(const component::PartMap& map, const ToolSettings& settings, component::Cache& cache, uint32_t part)
    {
        T defaultWght{ 1.0 };
        if (map.empty())
            return defaultWght;

        auto val = cache.get(part);
        if (val)
            return settings.scale * val.value();

        auto cacheAndReturn = [&](T w)
        {
            cache.set(part, w);
            return w * settings.scale;
        };

        CLxEaseFraction remap;
        CLxPerlin<T>    noise(4, 1, 1, settings.seed);
        CLxVector       v;

        auto* partData = map.get(part);
        assert(partData);

        switch (settings.mode)
        {
            case global::FalloffMode::Random:
                return cacheAndReturn(noise.eval(partData->center));

            case global::FalloffMode::Position:
                v = settings.maxPos - settings.minPos;
                remap.set_shape(LXiESHP_LINEAR);

                auto offsetVec = partData->center - settings.minPos;
                auto num       = offsetVec.dot(v);
                auto den       = v.lengthSquared();
                if (den)
                    return cacheAndReturn(remap.evaluate(num / den));
        }

        return cacheAndReturn(defaultWght);
    }

    struct DrawInfo
    {
        CLxVector  startPos;
        CLxVector  endPos;
        CLxVector  eyeVector;
        double     height3D;
        LXtVector4 color;
    };

    static void drawSteps(ILxUnknownID drawIFC, const DrawInfo& drawInfo)
    {
        CLxUser_StrokeDraw stroke(drawIFC);
        CLxUser_View       view(drawIFC);

        if (!stroke.test())
        {
            // This shbould never happen,
            // but apparently it does!
            assert(false);
            return;
        }

        // We want to draw steps that start at the end and go down to the start..
        // We need to know the direction to offset the step height, which is the
        // cross between the start-end vector and the view's eye vector.
        CLxVector delta = drawInfo.endPos - drawInfo.startPos;
        double    dLen  = delta.length();

        delta.normalize();
        auto upVec = drawInfo.eyeVector.cross(delta);
        upVec.normalize();

        double stepWidth  = dLen / (drawing::Steps);
        double stepHeight = drawInfo.height3D / (drawing::Steps);

        // stroke.SetPart(global::id::steps);
        stroke.Begin(LXiSTROKE_LINE_LOOP, drawInfo.color, drawInfo.color[3]);
        stroke.Vert(const_cast<double*>(drawInfo.endPos.v));

        CLxVector fullOffset = upVec * drawInfo.height3D;
        stroke.Vert(fullOffset, LXiSTROKE_RELATIVE);

        CLxVector stepRun  = delta * stepWidth;
        CLxVector stepRise = upVec * stepHeight;
        CLxVector runInv   = stepRun * -1.0;
        CLxVector riseInv  = stepRise * -1.0;
        for (auto i = 0; i < drawing::Steps; ++i)
        {
            stroke.Vert(runInv, LXiSTROKE_RELATIVE);
            stroke.Vert(riseInv, LXiSTROKE_RELATIVE);
        }

        stroke.Vert(riseInv, LXiSTROKE_RELATIVE);
        for (auto i = 0; i < drawing::Steps; ++i)
        {
            stroke.Vert(stepRun, LXiSTROKE_RELATIVE);
            if (i != (drawing::Steps - 1))
                stroke.Vert(riseInv, LXiSTROKE_RELATIVE);
        }
    }

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

        static void item()
        {
            CLxGenericPolymorph* srv = new CLxPolymorph<falloffItem::Package>;
            srv->AddInterface(new CLxIfc_Package<falloffItem::Package>);
            srv->AddInterface(new CLxIfc_StaticDesc<falloffItem::Package>);
            lx::AddServer(global::id::package.c_str(), srv);

            srv = new CLxPolymorph<falloffItem::Instance>;
            srv->AddInterface(new CLxIfc_PackageInstance<falloffItem::Instance>);
            srv->AddInterface(new CLxIfc_ViewItem3D<falloffItem::Instance>);
            lx::AddSpawner(global::id::instance.c_str(), srv);

            CLxSpawner_Falloff<falloffItem::Falloff>(global::id::falloff.c_str());

            CLxExport_ItemModifierServer<CLxObjectRefModifier<falloffItem::Modifier>>::Define(global::id::modifier.c_str());
        }
    }  // namespace init

    namespace spawn
    {
        static partFalloff::Packet* packet(void** ppvObj)
        {
            static CLxSpawner<partFalloff::Packet> spawner(global::id::packet.c_str());
            return spawner.Alloc(ppvObj);
        }

        static falloffItem::Instance* instance(void** ppvObj)
        {
            static CLxSpawner<falloffItem::Instance> spawner(global::id::instance.c_str());
            return spawner.Alloc(ppvObj);
        }

        static falloffItem::Falloff* falloff(ILxUnknownID& obj)
        {
            static CLxSpawner<falloffItem::Falloff> spawner(global::id::falloff.c_str());
            return spawner.Alloc(obj);
        }

    }  // namespace spawn

    namespace test
    {
        static LxResult instance(const LXtGUID* guid)
        {
            static CLxSpawner<falloffItem::Instance> spawner(global::id::instance.c_str());
            return spawner.TestInterfaceRC(guid);
        }
    }  // namespace test
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
        std::scoped_lock<std::mutex> scopeLock(m_lock);
        auto                         it = m_weights.find(part);
        return it != m_weights.end() ? it->second : std::optional<double>{};
    }

    void Cache::set(uint32_t part, double weight)
    {
        std::scoped_lock<std::mutex> scopeLock(m_lock);
        m_weights[part] = weight;
    }

    void Cache::clear()
    {
        std::scoped_lock<std::mutex> scopeLock(m_lock);
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

    void Tool::tmod_Draw(ILxUnknownID, ILxUnknownID stroke, int)
    {
        if (getAttr<int>(global::attrs::mode) != static_cast<int>(global::FalloffMode::Position))
            return;

        CLxUser_HandleDraw draw(stroke);

        auto start = getAttr<CLxVector>(global::attrs::startX);
        draw.Handle(start.v, nullptr, global::id::startPt, 0);

        auto end = getAttr<CLxVector>(global::attrs::endX);
        draw.Handle(end.v, nullptr, global::id::endPt, 0);

        CLxUser_View view(stroke);
        if (!view.test())
        {
            assert(false);
            return;
        }

        global::DrawInfo info;
        info.startPos = start;
        info.endPos   = end;
        info.height3D = global::drawing::PixelWidth * view.PixelScale();
        LXx_VCPY(info.color, global::drawing::ToolColor);
        info.color[3] = 1.0;

        CLxVector midPt = (end + start) / 2.0;
        view.EyeVector(midPt.v, info.eyeVector.v);

        global::drawSteps(stroke, info);
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

        uint32_t part;
        m_pointAcc.Select(vrx);
        m_pointAcc.Part(&part);

        return global::evalFalloff<double>(m_partData, m_settings, m_weightCache, part);
    }

}  // namespace partFalloff

namespace falloffItem
{
    LxResult Package::pkg_SetupChannels(ILxUnknownID addChan)
    {
        CLxUser_AddChannel ac(addChan);

        ac.NewChannel(global::attrs::mode.c_str(), LXsTYPE_INTEGER);
        ac.SetDefault(0.0, 0);
        ac.SetHint(global::falloffModes);

        ac.NewChannel(global::attrs::start.c_str(), LXsTYPE_DISTANCE);
        ac.SetVector(LXsCHANVEC_XYZ);

        CLxVector endDefault(0.0, 1.0, 0.0);
        ac.NewChannel(global::attrs::end.c_str(), LXsTYPE_DISTANCE);
        ac.SetVector(LXsCHANVEC_XYZ);
        ac.SetDefaultVec(endDefault);

        ac.NewChannel(global::attrs::seed.c_str(), LXsTYPE_INTEGER);
        ac.SetDefault(0.0, 1701);

        ac.NewChannel(global::attrs::scale.c_str(), LXsTYPE_PERCENT);
        ac.SetDefault(1.0, 0);
        return LXe_OK;
    }

    LxResult Package::pkg_TestInterface(const LXtGUID* guid)
    {
        return com::test::instance(guid);
    }

    LxResult Package::pkg_Attach(void** ppvObj)
    {
        com::spawn::instance(ppvObj);
        return LXe_OK;
    }

    LXtTagInfoDesc Package::descInfo[] = { { LXsPKG_SUPERTYPE, LXsITYPE_FALLOFF }, { nullptr } };

    LxResult Instance::pins_Initialize(ILxUnknownID item, ILxUnknownID)
    {
        m_item.set(item);
        return LXe_OK;
    }

    void Instance::pins_Cleanup(void)
    {
        m_item.clear();
    }

    LxResult Instance::vitm_Draw(ILxUnknownID chanRead, ILxUnknownID draw, int sel, const LXtVector color)
    {
        CLxUser_ChannelRead read(chanRead);
        assert(read.test() && m_item.test());

        auto readVec = [&](CLxVector& v, uint32_t idx)
        {
            for (auto i = 0; i < 3; ++i)
                v[i] = read.FValue(m_item, idx++);
        };

        global::DrawInfo info;

        readVec(info.startPos, m_item.ChannelIndex(global::attrs::startX.c_str()));
        readVec(info.endPos, m_item.ChannelIndex(global::attrs::endX.c_str()));
        info.height3D = 0.25;
        info.eyeVector.set(1.0, 0.0, 0.0);
        LXx_VCPY(info.color, color);
        info.color[3] = sel == 0 ? 0.5 : 1.0;

        global::drawSteps(draw, info);
        info.eyeVector.set(0.0, 0.0, 1.0);
        global::drawSteps(draw, info);

        return LXe_OK;
    }

    float Falloff::fall_WeightF(const LXtFVector position, LXtPointID vrx, LXtPolygonID polygon)
    {
        if (!vrx || !m_pointAcc.test() || m_partData.empty())
            return 1.0;

        uint32_t part{};
        m_lock.lock();
        m_pointAcc.Select(vrx);
        m_pointAcc.Part(&part);
        m_lock.unlock();

        return global::evalFalloff<float>(m_partData, settings, m_weightCache, part);
    }

    LxResult Falloff::fall_WeightRun(const float** pos, const LXtPointID* points, const LXtPolygonID* polygons, float* weight, unsigned num)
    {
        for (auto i = 0u; i < num; ++i)
            weight[i] = fall_WeightF(pos[i], points[i], polygons ? polygons[0] : nullptr);

        return LXe_OK;
    }

    LxResult Falloff::fall_SetMesh(ILxUnknownID meshObj, LXtMatrix4 xfrm)
    {
        std::lock_guard<std::mutex> scopeLock(m_lock);
        CLxUser_Mesh                mesh(meshObj);
        if (!mesh.test())
            return LXe_FAILED;
        m_partData.buildFromMesh(mesh);
        m_pointAcc.fromMesh(mesh);

        return LXe_OK;
    }

    const char* Modifier::ItemType()
    {
        return global::id::package.c_str();
    }

    const char* Modifier::Channel()
    {
        return LXsICHAN_FALLOFF_FALLOFF;
    }

    void Modifier::Attach(CLxUser_Evaluation& eval, ILxUnknownID itemObj)
    {
        CLxUser_Item item(itemObj);

        eval.AddChan(item, LXsICHAN_XFRMCORE_WORLDMATRIX);

        for (auto const& [name, type] : global::attrs::typeMap)
            eval.AddChan(item, name.c_str());
    }

    void Modifier::Alloc(CLxUser_Evaluation&, CLxUser_Attributes& attr, unsigned firstIdx, ILxUnknownID& obj)
    {
        uint32_t idx = firstIdx;

        auto readVec = [&]()
        {
            CLxVector v;
            for (auto i = 0; i < 3; ++i)
                attr.GetFlt(idx++, &v.v[i]);
            return v;
        };

        CLxMatrix4     mat;
        CLxUser_Matrix m;
        attr.ObjectRO(idx++, m);
        m.Get4(mat.m);

        auto* falloff = com::spawn::falloff(obj);

        falloff->settings.mode   = static_cast<global::FalloffMode>(attr.Int(idx++));
        falloff->settings.minPos = mat * readVec();
        falloff->settings.minPos += mat.getTranslation();
        falloff->settings.maxPos = mat * readVec();
        falloff->settings.maxPos += mat.getTranslation();
        falloff->settings.scale = attr.Float(idx++);
        falloff->settings.seed  = attr.Int(idx++);
    }

}  // namespace falloffItem

void initialize()
{
    com::init::tool();
    com::init::packet();
    com::init::item();
}
