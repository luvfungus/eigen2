#pragma once

#include "internal/WorkCoordinator.h"
#include "core/RefCounted.h"
#include "core/PodDeque.h"
#include "core/Error.h"
#include "Worklist.h"
#include "RenderPlan.h"
#include "Display.h"
#include "Texture.h"
#include "TargetSet.h"
#include "RenderPort.h"
#include <thread>

namespace eigen
{

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class Renderer
    {
    public:
                                struct PlatformConfig;

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //
        // User-facing API
        //

        struct Config
        {
            Allocator*          allocator       = Mallocator::Get();
            bool                debugEnabled    = false;
            unsigned            scratchSize     = 16*1024*1024;
            unsigned            submitThreads   = 1;
            PlatformConfig*     platformConfig  = nullptr;
        };

                                Renderer();
                               ~Renderer();

        Error                   initialize(const Config& config);
        void                    cleanup();          // Optional, handled by dtor

        Worklist*               openWorklist(RenderPlan* plan);

        void                    commenceWork();

        RenderPort*             getPort(const char* name);
        unsigned                getFrameNumber() const;

        DisplayPtr              createDisplay();
        TexturePtr              createTexture();
        TargetSetPtr            createTargetSet();
        RenderPlanPtr           createPlan();

        //
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

                                struct PlatformDetails;
    protected:

        friend void             DestroyRefCounted(Display*);
        friend void             DestroyRefCounted(Texture*);
        friend void             DestroyRefCounted(TargetSet*);
        friend void             DestroyRefCounted(RenderPlan*);

        struct DeadMeat
        {
            void*               object;
            DeleteFunc          deleteFunc;
            unsigned            frameNumber;
        };

        Error                   platformInit(const Config& config);
        void                    platformCleanup();

                                template<class T>
        void                    scheduleDeletion(T* obj, unsigned delay);

        enum {                  MaxWorklists = 12 };

        Config                  _config;

        BlockAllocator          _displayAllocator;
        BlockAllocator          _textureAllocator;
        BlockAllocator          _targetSetAllocator;
        RenderPlanManager       _planManager;
        Keysmith<RenderPort>    _portSmith;
        PodDeque<DeadMeat>      _deadMeat;

        int8_t*                 _scratchMem         = 0;

        std::atomic<int8_t*>    _scratchAllocPtr    = 0;
        int8_t*                 _scratchAllocEnd    = 0;

        Worklist                _worklists[MaxWorklists];
        unsigned                _worklistStart      = 0;
        unsigned                _worklistEnd        = 0;
        unsigned                _worklistEndVacant  = MaxWorklists-1;
        WorkCoordinator         _workCoordinator;
        std::thread             _workSubmissionThread;

        unsigned                _frameNumber        = 0;

        void*                   _platformDetails[8];

    public:

        PlatformDetails&        getPlatformDetails();
        RenderPlanManager&      getPlanManager();
        int8_t*                 scratchAlloc(uintptr_t bytes);

    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    inline Renderer::Renderer()
    {
    }

    inline RenderPort* Renderer::getPort(const char* name) throw()
    {
        return _portSmith.issue(name);
    }

    inline unsigned Renderer::getFrameNumber() const
    {
        return _frameNumber;
    }

    inline int8_t* Renderer::scratchAlloc(uintptr_t bytes)
    {
        int8_t* p = _scratchAllocPtr.fetch_add((bytes + 15) & ~15, std::memory_order_relaxed);  // p gets old value
        if (p + bytes > _scratchAllocEnd)
        {
            return nullptr;
        }
        return p;
    }

    template<class T> void Renderer::scheduleDeletion(T* obj, unsigned delay)
    {
        assert(delay > 0);
        Renderer::DeadMeat& deadMeat = _deadMeat.addLast();
        deadMeat.object = obj;
        deadMeat.deleteFunc = (DeleteFunc)Delete<T>;
        deadMeat.frameNumber = _frameNumber + delay;
    }

    inline RenderPlanManager& Renderer::getPlanManager()
    {
        return _planManager;
    }

}

