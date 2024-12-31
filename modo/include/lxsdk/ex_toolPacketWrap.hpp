#pragma once

// For whatever reason, some of the modeling tool packets never had wrappers exposed.
// This header adds wrappers for writing direct modeling falloffs, action centers, and
// action axes.

#include <lxsdk/lx_wrap.hpp>
#include <lxsdk/lxw_tool.hpp>

class CLxImpl_FalloffPacket
{
public:
    virtual ~CLxImpl_FalloffPacket() = default;

    virtual double fp_Evaluate([[maybe_unused]] LXtFVector pos, [[maybe_unused]] LXtPointID vrx, [[maybe_unused]] LXtPolygonID poly)
    {
        return 1.0;
    }

    virtual double fp_Screen([[maybe_unused]] LXtObjectID vts, [[maybe_unused]] int x, [[maybe_unused]] int y)
    {
        return 1.0;
    }
};

template <class T>
class CLxIfc_FalloffPacket : public CLxInterface
{
public:
    CLxIfc_FalloffPacket()
    {
        vt.Evaluate = Evaluate;
        vt.Screen   = Screen;
        vTable      = &vt.iunk;
        iid         = &lx::guid_FalloffPacket;
    }

    static auto Evaluate(LXtObjectID wcom, LXtFVector pos, LXtPointID vrt, LXtPolygonID poly) -> double
    {
        LXCWxINST(CLxImpl_FalloffPacket, loc);
        return loc->fp_Evaluate(pos, vrt, poly);
    }

    static auto Screen(LXtObjectID wcom, LXtObjectID vts, int x, int y) -> double
    {
        LXCWxINST(CLxImpl_FalloffPacket, loc);
        return loc->fp_Screen(vts, x, y);
    }

private:
    ILxFalloffPacket vt;
};
