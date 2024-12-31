#pragma once

#include <lxsdk/lx_action.hpp>
#include <lxsdk/lx_vertex.hpp>
#include <lxsdk/lxu_matrix.hpp>
#include <lxsdk/lxu_vector.hpp>

#include <lxsdk/lx_particle.hpp>
#include <lxsdk/lx_tableau.hpp>

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Header-only singleton for reading particle sources.  See example
// meshop for usage.

// Declarations with docs up top
namespace particleAPI
{
    /// The parser is the low level object that interacts with the SDK.
    /// Curious users can look at it, but generally the derived "ParticleCollection"
    /// class is what clients interact with.
    class Parser : public CLxImpl_TriangleSoup, public CLxSingletonPolymorph
    {
    public:
        Parser();
        virtual ~Parser() = default;

        /// The SingletonPolymorph method allows us to spawn the COM object in our constructor.
        /// COM stuff that clients shouldn't have to think about.
        LXxSINGLETON_METHOD;

        /// Sampling creates a vertex descriptor with the features we're interested in, then uses
        /// the TriangleSoup interface to parse the particle source.  This is the only public
        /// entrypoint to the low level object on the client's side, but we'll wrap it as the
        /// difference between Surfaces, SurfaceItems, ParticleItems, TableauSurfs, etc is confusing.
        LxResult sample(CLxUser_TableauSurface& bin);

        /// When a surface/particle source is sampled, it first sends the index and type of the
        /// next segment so that the client doing the sampling can choose to skip it if desired.
        /// For a particle source, we only care about POINT segments, obviously.
        LxResult soup_Segment(unsigned int, unsigned int type) override;

        /// For each Vertex/Particle, the surf/particles will call this function with the array
        /// of data.  The client can generate and return an index for this vert, but it is
        /// never used for particle sources.
        LxResult soup_Vertex(const float* vertex, unsigned int*) override;

        /// If the segment is a polygon, the surf/particles will call this to pass the indices
        /// of the verts that compose the given triangle.  Because of a bug I never fixed, we
        /// have to implement this but don't actually need to do anything besides return ok.
        LxResult soup_Polygon(unsigned int, unsigned int, unsigned int) override;

        // Everything else is just the interface for the ParticleCollection to implement.
        virtual int                       featureOffset(const std::string&)                                  = 0;
        virtual int                       featureSize(const std::string&)                                    = 0;
        virtual void                      addFilter(const std::string&)                                      = 0;
        virtual const uint32_t            particleCount() const                                              = 0;
        virtual float const*              particleByIndex(uint32_t, uint32_t*) const                         = 0;
        virtual float const*              particleAttrByIndex(const std::string&, uint32_t, uint32_t*) const = 0;
        virtual const std::vector<float>& particleValues(uint32_t*) const                                    = 0;
        virtual const std::vector<float>& attrValues(const std::string&, uint32_t*)                          = 0;

    protected:
        // Set of attributes to include
        std::unordered_set<std::string> m_attrFilter;
        // Map of float counts for each attr.
        std::unordered_map<std::string, uint32_t> m_attrSizes;
        // Map of offsets into array for each attr.
        std::unordered_map<std::string, uint32_t> m_attrOffsets;
        // Particle array stride / sizeof(float)
        uint32_t m_vDescSize{};
        // The flat vector for all values
        std::vector<float> m_allValues;
        // The optionally populated map of vectors for each attribute
        std::unordered_map<std::string, std::vector<float>> m_attrValues;
    };

    class ParticleCollection final : public Parser
    {
    public:
        /// The Parsers map of the read attributes and their respective offsets
        /// in the particle array can be accessed if needed.  In an array such as:
        /// "p1_pos.x, p1_pos.y, p1_pos.z, p1_size, p2_pos.x, p2_pos.y, etc...""
        /// the returned offset for size should be 3, since the value for size is offset by
        /// 3 floats in the array.
        /// \returns the offset for a given attribute or -1 if the attribute isn't found.
        int featureOffset(const std::string& attrName) override;

        /// The Parsers map of the read attributes and their respective sizes (float counts)
        /// in the particle array can be accessed if needed.  In an array such as:
        /// "p1_pos.x, p1_pos.y, p1_pos.z, p1_mass, p2_pos.x, p2_pos.y, etc..."
        /// the returned size for position would be 3 (3 floats), vs. mass which would be 1.
        /// \returns the size of a given attribute or -1 if the attribute isn't found.
        int featureSize(const std::string& attrName) override;

        /// Clients can choose to only read and store specific attributes by name.  If no
        /// filters are added, all attributes contained in the particle source will be read
        /// and stored, otherwise only the attributes that match the names that have been added
        /// as filters will be read.
        void addFilter(const std::string& attrName) override;

        /// \returns The number of parsed particles.
        const uint32_t particleCount() const override;

        /// Access the float array composed of all packed attribute values for a given particle.
        /// \returns a pointer to the first float for a given particle, and optionally the number
        /// of floats that make up that particle's data.
        float const* particleByIndex(uint32_t index, uint32_t* count) const override;

        /// Access only the floats representing a given attribute for a given particle.  The size returned
        /// is the
        /// \returns a pointer to the first float for the attribute on a given particle, and optionally
        /// the number of floats in the returned array (eg 3 for position, 1 for mass, etc)
        float const* particleAttrByIndex(const std::string& attrName, uint32_t idx, uint32_t* size) const override;

        /// Access the full, packed array of all attributes for all particles.
        /// \returns the packed vector of all values for all particles.  Optionally
        /// returns the number of floats per-particle.
        const std::vector<float>& particleValues(uint32_t* pfSize) const override;

        /// By default, particle values are stored in a packed, non-unit stride array.
        /// eg "p1_pos.x, p1_pos.y, p1_pos.z, p1_mass, p2_pos.x, p2_pos.y, etc..."
        /// In cases where all values of a given attribute will be looped through, the
        /// data can be sorted into discrete vectors for each attribute (eg p1_size, p2_size, etc).
        /// These vectors will be cached so this increases memory use.
        /// \returns the vector of floats for a given attribute.
        const std::vector<float>& attrValues(const std::string& attrName, uint32_t* size) override;

    private:
        /// We keep an empty vector of floats for attributes that don't exist.
        std::vector<float> m_empty;
    };

    /// The EvalReader wraps the functionality needed to read a particle source from a modifier.
    /// Clients can call attach in their initialization method, followed by addAttr to limit the
    /// number of particle features being read.  Finally in the modifier's eval method, they can call
    /// read and get a populated ParticleCollection object returned.
    class EvalReader
    {
    public:
        /// Call attach with the particle source item and the eval object to ensure your modifier's eval
        /// method is called anytime the particle source changes.
        LxResult attach(CLxUser_Evaluation& eval, CLxUser_Item& item);

        /// By default, all attributes (size, position, velocity, etc) for a given particle source will
        /// be read and stored.  If only specific attributes are needed, they can be added one by one
        /// with the addAttr function.
        void addAttr(const std::string& attrName);

        /// Since we are outside of the land of COM here, we return a shared_ptr
        /// to the particle collection instead of a reference counted COM object.
        /// This is pretty weird for modo, but really it's just another way to do a
        /// reference counted RAII wrapper around a pointer.
        std::shared_ptr<ParticleCollection> read(CLxUser_Attributes& attr);

    private:
        std::shared_ptr<ParticleCollection> m_reader{};
        CLxUser_ParticleItem                m_pItem;
        uint32_t                            m_pIdx;
    };
}  // namespace particleAPI

// Implementations
namespace particleAPI
{
    // Low level Parser implementation
    inline Parser::Parser()
    {
        AddInterface(new CLxIfc_TriangleSoup<Parser>);
    }

    inline LxResult Parser::sample(CLxUser_TableauSurface& bin)
    {
        if (!bin.test())
            return LXe_FAILED;

        auto const nFeatures = bin.FeatureCount(LXiTBLX_PARTICLES);
        if (!nFeatures)
            return LXe_FAILED;

        // Allocate a vert descriptor
        CLxUser_TableauService tabSvc;
        CLxUser_TableauVertex  vDesc;

        if (!tabSvc.NewVertex(vDesc))
            return LXe_FAILED;

        m_allValues.clear();
        m_attrValues.clear();
        m_attrSizes.clear();
        m_attrOffsets.clear();

        bool takeAllAttrs = m_attrFilter.empty();

        // Walk the bin and create entries in our maps for each
        // particle attribute it holds (position, velocity, etc)
        CLxUser_VertexFeatureService vfSvc;
        for (auto i = 0u; i < nFeatures; i++)
        {
            // Need the name to find the offset in the particle array for the attr.
            const char* fName{};
            if (LXx_FAIL(bin.FeatureByIndex(LXiTBLX_PARTICLES, i, &fName)))
                continue;

            // Need the ident to find the dimension (how many floats) of the attr.
            const char* fIdent{};
            if (LXx_FAIL(vfSvc.Lookup(LXiTBLX_PARTICLES, fName, &fIdent)))
                continue;

            if (!takeAllAttrs && m_attrFilter.find(fName) == m_attrFilter.end() && m_attrFilter.find(fIdent) == m_attrFilter.end())
                continue;

            // Now add the attr/feature to our vert descriptor obj
            uint32_t idx;
            if (LXx_OK(vDesc.AddFeature(LXiTBLX_PARTICLES, fName, &idx)))
            {
                m_attrOffsets[fName] = vDesc.GetOffset(LXiTBLX_PARTICLES, fName);

                unsigned dim;
                vfSvc.Dimension(fIdent, &dim);
                m_attrSizes[fName] = dim;
            }
        }

        m_vDescSize = vDesc.Size();
        if (!m_vDescSize)
            return LXe_FAILED;

        auto rc = bin.SetVertex(vDesc);
        return LXx_OK(rc) ? bin.Sample(nullptr, 1.0, *this) : rc;
    }

    inline LxResult Parser::soup_Segment(unsigned int, unsigned int type)
    {
        return (type == LXiTBLX_SEG_POINT) ? LXe_TRUE : LXe_FALSE;
    }

    inline LxResult Parser::soup_Vertex(const float* vertex, unsigned int*)
    {
        m_allValues.insert(m_allValues.end(), vertex, vertex + m_vDescSize);
        return LXe_OK;
    }

    inline LxResult Parser::soup_Polygon(unsigned int, unsigned int, unsigned int)
    {
        return LXe_OK;
    }

    // Implementation of the ParticleCollection

    inline int ParticleCollection::featureOffset(const std::string& attrName)
    {
        auto it = m_attrOffsets.find(attrName);
        return it == m_attrOffsets.end() ? -1 : it->second;
    }

    inline int ParticleCollection::featureSize(const std::string& attrName)
    {
        auto it = m_attrSizes.find(attrName);
        return it == m_attrSizes.end() ? -1 : it->second;
    }

    inline void ParticleCollection::addFilter(const std::string& attrName)
    {
        m_attrFilter.insert(attrName);
    }

    inline const uint32_t ParticleCollection::particleCount() const
    {
        if (m_vDescSize > 0)
            return static_cast<uint32_t>(m_allValues.size() / m_vDescSize);

        return 0u;
    }

    inline float const* ParticleCollection::particleByIndex(uint32_t idx, uint32_t* count) const
    {
        if (count)
            count[0] = m_vDescSize;

        return &m_allValues[idx * m_vDescSize];
    }

    inline float const* ParticleCollection::particleAttrByIndex(const std::string& attrName, uint32_t idx, uint32_t* size) const
    {
        auto it = m_attrOffsets.find(attrName);
        if (it == m_attrOffsets.end())
        {
            if (size)
                size[0] = 0u;

            return nullptr;
        }

        if (size)
            size[0] = m_attrSizes.at(attrName);

        return &m_allValues[idx * m_vDescSize + it->second];
    }

    inline const std::vector<float>& ParticleCollection::particleValues(uint32_t* pfSize) const
    {
        if (pfSize)
            pfSize[0] = m_vDescSize;

        return m_allValues;
    }

    inline const std::vector<float>& ParticleCollection::attrValues(const std::string& attrName, uint32_t* size)
    {
        if (m_attrValues.empty())
        {
            for (const auto& [name, attrSize] : m_attrSizes)
            {
                std::vector<float> attrVals;
                auto               offset = m_attrOffsets.at(name);

                for (auto i = 0u; i < m_allValues.size(); i += m_vDescSize)
                {
                    auto start = m_allValues.begin() + i * m_vDescSize + offset;
                    attrVals.insert(attrVals.end(), start, start + attrSize);
                }
                m_attrValues[name] = std::move(attrVals);
            }
        }

        auto it = m_attrValues.find(attrName);
        if (it == m_attrValues.end())
            return m_empty;

        if (size)
            size[0] = m_attrSizes.at(attrName);

        return it->second;
    }

    // EvalReader implementation

    inline LxResult EvalReader::attach(CLxUser_Evaluation& eval, CLxUser_Item& item)
    {
        if (!m_pItem.set(item))
            return LXe_FAILED;

        m_reader = std::make_shared<ParticleCollection>();
        return m_pItem.Prepare(eval, &m_pIdx);
    }

    inline void EvalReader::addAttr(const std::string& attrName)
    {
        if (!m_reader)
            throw(LXe_NOTREADY);

        m_reader.get()->addFilter(attrName);
    }

    inline std::shared_ptr<ParticleCollection> EvalReader::read(CLxUser_Attributes& attr)
    {
        CLxUser_TableauSurface bin;

        if (LXx_OK(m_pItem.Evaluate(attr, m_pIdx, bin)) && bin.test())
        {
            m_reader.get()->sample(bin);
        }

        return m_reader;
    }

};  // namespace particleAPI
