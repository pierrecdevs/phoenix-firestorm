/**
 * @file llmeshrepository.h
 * @brief Client-side repository of mesh assets.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010-2013, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_MESH_REPOSITORY_H
#define LL_MESH_REPOSITORY_H

#include <unordered_map>
#include <unordered_set>
#include "llassettype.h"
#include "llmodel.h"
#include "lluuid.h"
#include "llviewertexture.h"
#include "llvolume.h"
#include "lldeadmantimer.h"
#include "httpcommon.h"
#include "httprequest.h"
#include "httpoptions.h"
#include "httpheaders.h"
#include "httphandler.h"
#include "llthread.h"

#define LLCONVEXDECOMPINTER_STATIC 1

#include "llconvexdecomposition.h"
#include "lluploadfloaterobservers.h"

class LLVOVolume;
class LLMutex;
class LLCondition;
class LLMeshRepository;

typedef enum e_mesh_processing_result_enum
{
    MESH_OK = 0,
    MESH_NO_DATA = 1,
    MESH_OUT_OF_MEMORY,
    MESH_HTTP_REQUEST_FAILED,
    MESH_PARSE_FAILURE,
    MESH_INVALID,
    MESH_UNKNOWN
} EMeshProcessingResult;

typedef enum e_mesh_request_type_enum
{
    MESH_REQUEST_HEADER,
    MESH_REQUEST_LOD,
    MESH_REQUEST_SKIN,
    MESH_REQUEST_DECOMPOSITION,
    MESH_REQUEST_PHYSICS,
    MESH_REQUEST_UKNOWN
} EMeshRequestType;

class LLMeshUploadData
{
public:
    LLPointer<LLModel> mBaseModel;
    LLPointer<LLModel> mModel[5];
    LLUUID mUUID;
    U32 mRetries;
    std::string mRSVP;
    std::string mAssetData;
    LLSD mPostData;

    LLMeshUploadData()
    {
        mRetries = 0;
    }
};

class LLTextureUploadData
{
public:
    LLViewerFetchedTexture* mTexture;
    LLUUID mUUID;
    std::string mRSVP;
    std::string mLabel;
    U32 mRetries;
    std::string mAssetData;
    LLSD mPostData;

    LLTextureUploadData()
    {
        mRetries = 0;
    }

    LLTextureUploadData(LLViewerFetchedTexture* texture, std::string& label)
        : mTexture(texture), mLabel(label)
    {
        mRetries = 0;
    }
};

class LLPhysicsDecomp : public LLThread
{
public:

    typedef std::map<std::string, LLSD> decomp_params;

    class Request : public LLRefCount
    {
    public:
        //input params
        S32* mDecompID;
        std::string mStage;
        std::vector<LLVector3> mPositions;
        std::vector<U16> mIndices;
        decomp_params mParams;

        //output state
        std::string mStatusMessage;
        std::vector<LLModel::PhysicsMesh> mHullMesh;
        LLModel::convex_hull_decomposition mHull;

        //status message callback, called from decomposition thread
        virtual S32 statusCallback(const char* status, S32 p1, S32 p2) = 0;

        //completed callback, called from the main thread
        virtual void completed() = 0;

        virtual void setStatusMessage(const std::string& msg);

        bool isValid() const {return mPositions.size() > 2 && mIndices.size() > 2 ;}

    protected:
        //internal use
        LLVector3 mBBox[2] ;
        F32 mTriangleAreaThreshold ;

        void assignData(LLModel* mdl) ;
        void updateTriangleAreaThreshold() ;
        bool isValidTriangle(U16 idx1, U16 idx2, U16 idx3) ;
    };

    LLCondition* mSignal;
    LLMutex* mMutex;

    bool mInited;
    bool mQuitting;
    bool mDone;

    LLPhysicsDecomp();
    ~LLPhysicsDecomp();

    void shutdown();

    void submitRequest(Request* request);
    static S32 llcdCallback(const char*, S32, S32);

    void setMeshData(LLCDMeshData& mesh, bool vertex_based);
    void doDecomposition();
    void doDecompositionSingleHull();

    virtual void run();

    void completeCurrent();
    void notifyCompleted();

    std::map<std::string, S32> mStageID;

    typedef std::queue<LLPointer<Request> > request_queue;
    request_queue mRequestQ;

    LLPointer<Request> mCurRequest;

    std::queue<LLPointer<Request> > mCompletedQ;

};

class RequestStats
{
public:

    RequestStats() :mRetries(0) {};

    void updateTime();
    bool canRetry() const;
    bool isDelayed() const;
    U32 getRetries() { return mRetries; }

private:
    U32 mRetries;
    LLFrameTimer mTimer;
};

class MeshLoadData;
class PendingRequestBase
{
public:
    struct CompareScoreGreater
    {
        bool operator()(const std::shared_ptr<PendingRequestBase>& lhs, const std::shared_ptr<PendingRequestBase>& rhs)
        {
            return lhs->mScore > rhs->mScore; // greatest = first
        }
    };

    PendingRequestBase() : mScore(0.f), mTrackedData(nullptr), mScoreDirty(true) {};
    virtual ~PendingRequestBase() {}

    bool operator<(const PendingRequestBase& rhs) const
    {
        return mId < rhs.mId;
    }

    F32 getScore() const { return mScore; }
    void checkScore()
    {
        constexpr F32 EXPIRE_TIME_SECS = 8.f;
        if (mScoreTimer.getElapsedTimeF32() > EXPIRE_TIME_SECS || mScoreDirty)
        {
            updateScore();
            mScoreDirty = false;
            mScoreTimer.reset();
        }
    };

    LLUUID getId() const { return mId; }
    virtual EMeshRequestType getRequestType() const = 0;

    void trackData(MeshLoadData* data) { mTrackedData = data; mScoreDirty = true; }
    void untrackData() { mTrackedData = nullptr; }
    bool hasTrackedData() { return mTrackedData != nullptr; }
    void setScoreDirty() { mScoreDirty = true; }

protected:
    void updateScore();

    LLUUID mId;
    F32 mScore;
    bool mScoreDirty;
    LLTimer mScoreTimer;
    MeshLoadData* mTrackedData;
};

class PendingRequestLOD : public PendingRequestBase
{
public:
    LLVolumeParams  mMeshParams;
    S32 mLOD;

    PendingRequestLOD(const LLVolumeParams& mesh_params, S32 lod)
        : PendingRequestBase(), mMeshParams(mesh_params), mLOD(lod)
    {
        mId = mMeshParams.getSculptID();
    }

    EMeshRequestType getRequestType() const override { return MESH_REQUEST_LOD; }
};

class PendingRequestUUID : public PendingRequestBase
{
public:

    PendingRequestUUID(const LLUUID& id, EMeshRequestType type)
        : PendingRequestBase(), mRequestType(type)
    {
        mId = id;
    }

    EMeshRequestType getRequestType() const override { return mRequestType; }

private:
    EMeshRequestType mRequestType;
};


class MeshLoadData
{
public:
    MeshLoadData() {}
    ~MeshLoadData()
    {
        if (std::shared_ptr<PendingRequestBase> request = mRequest.lock())
        {
            request->untrackData();
        }
    }
    void initData(LLVOVolume* vol, std::shared_ptr<PendingRequestBase>& request)
    {
        mVolumes.push_back(vol);
        request->trackData(this);
        mRequest = request;
    }
    void addVolume(LLVOVolume* vol)
    {
        mVolumes.push_back(vol);
        if (std::shared_ptr<PendingRequestBase> request = mRequest.lock())
        {
            request->setScoreDirty();
        }
    }
    std::vector<LLVOVolume*> mVolumes;
private:
    std::weak_ptr<PendingRequestBase> mRequest;
};

class LLMeshHeader
{
public:

    LLMeshHeader() {}

    explicit LLMeshHeader(const LLSD& header)
    {
        fromLLSD(header);
    }

    void fromLLSD(const LLSD& header)
    {
        const char* lod[] =
        {
            "lowest_lod",
            "low_lod",
            "medium_lod",
            "high_lod"
        };

        mVersion = header["version"].asInteger();

        for (U32 i = 0; i < 4; ++i)
        {
            mLodOffset[i] = header[lod[i]]["offset"].asInteger();
            mLodSize[i] = header[lod[i]]["size"].asInteger();
        }

        mSkinOffset = header["skin"]["offset"].asInteger();
        mSkinSize = header["skin"]["size"].asInteger();

        mPhysicsConvexOffset = header["physics_convex"]["offset"].asInteger();
        mPhysicsConvexSize = header["physics_convex"]["size"].asInteger();

        mPhysicsMeshOffset = header["physics_mesh"]["offset"].asInteger();
        mPhysicsMeshSize = header["physics_mesh"]["size"].asInteger();

        m404 = header.has("404");

        // <FS:Ansariel> DAE export
        if (header.has("creator") && header["creator"].isUUID())
        {
            mCreatorId = header["creator"].asUUID();
        }
        // </FS:Ansariel>
    }
private:

    enum EDiskCacheFlags {
        FLAG_SKIN = 1 << LLModel::NUM_LODS,
        FLAG_PHYSCONVEX = 1 << (LLModel::NUM_LODS + 1),
        FLAG_PHYSMESH = 1 << (LLModel::NUM_LODS + 2),
    };
public:
    U32 getFlags()
    {
        U32 flags = 0;
        for (U32 i = 0; i < LLModel::NUM_LODS; i++)
        {
            if (mLodInCache[i])
            {
                flags |= 1 << i;
            }
        }
        if (mSkinInCache)
        {
            flags |= FLAG_SKIN;
        }
        if (mPhysicsConvexInCache)
        {
            flags |= FLAG_PHYSCONVEX;
        }
        if (mPhysicsMeshInCache)
        {
            flags |= FLAG_PHYSMESH;
        }
        return flags;
    }

    void setFromFlags(U32 flags)
    {
        for (U32 i = 0; i < LLModel::NUM_LODS; i++)
        {
            mLodInCache[i] = (flags & (1 << i)) != 0;
        }
        mSkinInCache          = (flags & FLAG_SKIN) != 0;
        mPhysicsConvexInCache = (flags & FLAG_PHYSCONVEX) != 0;
        mPhysicsMeshInCache   = (flags & FLAG_PHYSMESH) != 0;
    }

    S32 mVersion = -1;
    S32 mSkinOffset = -1;
    S32 mSkinSize = -1;
    bool mSkinInCache = false;

    S32 mPhysicsConvexOffset = -1;
    S32 mPhysicsConvexSize = -1;
    bool mPhysicsConvexInCache = false;

    S32 mPhysicsMeshOffset = -1;
    S32 mPhysicsMeshSize = -1;
    bool mPhysicsMeshInCache = false;

    S32 mLodOffset[LLModel::NUM_LODS] = { -1 };
    S32 mLodSize[LLModel::NUM_LODS] = { -1 };
    bool mLodInCache[LLModel::NUM_LODS] = { false };
    S32 mHeaderSize = -1;

    bool m404 = false;

    // <FS:Ansariel> DAE export
    LLUUID mCreatorId{ LLUUID::null };
};

class LLMeshRepoThread : public LLThread
{
public:

    static std::atomic<S32> sActiveHeaderRequests;
    static std::atomic<S32> sActiveLODRequests;
    static std::atomic<S32> sActiveSkinRequests;
    static U32 sMaxConcurrentRequests;
    static S32 sRequestLowWater;
    static S32 sRequestHighWater;
    static S32 sRequestWaterLevel;          // Stats-use only, may read outside of thread

    LLMutex*    mMutex;
    LLMutex*    mHeaderMutex;
    LLMutex*    mLoadedMutex;
    LLMutex*    mPendingMutex;
    LLMutex*    mSkinMapMutex;
    LLCondition* mSignal;

    //map of known mesh headers
    typedef boost::unordered_map<LLUUID, LLMeshHeader> mesh_header_map; // pair is header_size and data
    mesh_header_map mMeshHeader;

    class HeaderRequest : public RequestStats
    {
    public:
        const LLVolumeParams mMeshParams;

        HeaderRequest(const LLVolumeParams&  mesh_params)
            : RequestStats(), mMeshParams(mesh_params)
        {
        }

        bool operator<(const HeaderRequest& rhs) const
        {
            return mMeshParams < rhs.mMeshParams;
        }
    };

    class LODRequest : public RequestStats
    {
    public:
        LLVolumeParams  mMeshParams;
        S32 mLOD;

        LODRequest(const LLVolumeParams&  mesh_params, S32 lod)
            : RequestStats(), mMeshParams(mesh_params), mLOD(lod)
        {
        }
    };

    class UUIDBasedRequest : public RequestStats
    {
    public:
        LLUUID mId;

        UUIDBasedRequest(const LLUUID& id)
            : RequestStats(), mId(id)
        {
        }

        bool operator<(const UUIDBasedRequest& rhs) const
        {
            return mId < rhs.mId;
        }
    };

    class LoadedMesh
    {
    public:
        LLPointer<LLVolume> mVolume;
        LLVolumeParams mMeshParams;
        S32 mLOD;

        LoadedMesh(LLVolume* volume, const LLVolumeParams&  mesh_params, S32 lod)
            : mVolume(volume), mMeshParams(mesh_params), mLOD(lod)
        {
        }

    };

    //set of requested skin info
    std::deque<UUIDBasedRequest> mSkinRequests;

    // list of completed skin info requests
    std::deque<LLPointer<LLMeshSkinInfo>> mSkinInfoQ;

    // list of skin info requests that have failed or are unavailaibe
    std::deque<UUIDBasedRequest> mSkinUnavailableQ;

    //set of requested decompositions
    std::set<UUIDBasedRequest> mDecompositionRequests;

    //set of requested physics shapes
    std::set<UUIDBasedRequest> mPhysicsShapeRequests;

    // list of completed Decomposition info requests
    std::list<LLModel::Decomposition*> mDecompositionQ;

    //queue of requested headers
    std::queue<HeaderRequest> mHeaderReqQ;

    //queue of requested LODs
    std::queue<LODRequest> mLODReqQ;

    //queue of unavailable LODs (either asset doesn't exist or asset doesn't have desired LOD)
    std::deque<LODRequest> mUnavailableQ;

    //queue of successfully loaded meshes
    std::deque<LoadedMesh> mLoadedQ;

    //map of pending header requests and currently desired LODs
    typedef std::unordered_map<LLUUID, std::array<S32, LLModel::NUM_LODS> > pending_lod_map;
    pending_lod_map mPendingLOD;

    // map of mesh ID to skin info (mirrors LLMeshRepository::mSkinMap)
    /// NOTE: LLMeshRepository::mSkinMap is accessed very frequently, so maintain a copy here to avoid mutex overhead
    typedef std::unordered_map<LLUUID, LLPointer<LLMeshSkinInfo>> skin_map;
    skin_map mSkinMap;

    // workqueue for processing generic requests
    LL::WorkQueue mWorkQueue;
    // lods have their own thread due to costly cacheOptimize() calls
    std::unique_ptr<LL::ThreadPool> mMeshThreadPool;

    // llcorehttp library interface objects.
    LLCore::HttpStatus                  mHttpStatus;
    LLCore::HttpRequest *               mHttpRequest;
    LLCore::HttpOptions::ptr_t          mHttpOptions;
    LLCore::HttpOptions::ptr_t          mHttpLargeOptions;
    LLCore::HttpHeaders::ptr_t          mHttpHeaders;
    LLCore::HttpRequest::policy_t       mHttpPolicyClass;
    LLCore::HttpRequest::policy_t       mHttpLegacyPolicyClass; // <FS:Ansariel> [UDP Assets]
    LLCore::HttpRequest::policy_t       mHttpLargePolicyClass;

    typedef std::unordered_set<LLCore::HttpHandler::ptr_t> http_request_set;
    http_request_set                    mHttpRequestSet;            // Outstanding HTTP requests

    // <FS:Ansariel> [UDP Assets]
    std::string mLegacyGetMeshCapability;
    std::string mLegacyGetMesh2Capability;
    int mLegacyGetMeshVersion;
    // </FS:Ansariel> [UDP Assets]
    std::string mGetMeshCapability;

    LLMeshRepoThread();
    ~LLMeshRepoThread();

    virtual void run();
    void cleanup();
    bool isShuttingDown() { return mShuttingDown; }

    void lockAndLoadMeshLOD(const LLVolumeParams& mesh_params, S32 lod);
    void loadMeshLOD(const LLVolumeParams& mesh_params, S32 lod);

    bool fetchMeshHeader(const LLVolumeParams& mesh_params);
    bool fetchMeshLOD(const LLVolumeParams& mesh_params, S32 lod);
    EMeshProcessingResult headerReceived(const LLVolumeParams& mesh_params, U8* data, S32 data_size, U32 flags = 0);
    EMeshProcessingResult lodReceived(const LLVolumeParams& mesh_params, S32 lod, U8* data, S32 data_size);
    bool skinInfoReceived(const LLUUID& mesh_id, U8* data, S32 data_size);
    bool decompositionReceived(const LLUUID& mesh_id, U8* data, S32 data_size);
    EMeshProcessingResult physicsShapeReceived(const LLUUID& mesh_id, U8* data, S32 data_size);
    bool hasPhysicsShapeInHeader(const LLUUID& mesh_id) const;
    bool hasSkinInfoInHeader(const LLUUID& mesh_id) const;
    bool hasHeader(const LLUUID& mesh_id) const;

    void notifyLoadedMeshes();
    S32 getActualMeshLOD(const LLVolumeParams& mesh_params, S32 lod);

    void loadMeshSkinInfo(const LLUUID& mesh_id);
    void loadMeshDecomposition(const LLUUID& mesh_id);
    void loadMeshPhysicsShape(const LLUUID& mesh_id);

    //send request for skin info, returns true if header info exists
    //  (should hold onto mesh_id and try again later if header info does not exist)
    bool fetchMeshSkinInfo(const LLUUID& mesh_id);

    //send request for decomposition, returns true if header info exists
    //  (should hold onto mesh_id and try again later if header info does not exist)
    bool fetchMeshDecomposition(const LLUUID& mesh_id);

    //send request for PhysicsShape, returns true if header info exists
    //  (should hold onto mesh_id and try again later if header info does not exist)
    bool fetchMeshPhysicsShape(const LLUUID& mesh_id);

    static void incActiveLODRequests();
    static void decActiveLODRequests();
    static void incActiveHeaderRequests();
    static void decActiveHeaderRequests();
    static void incActiveSkinRequests();
    static void decActiveSkinRequests();

    // Set the caps strings and preferred version for constructing
    // mesh fetch URLs.
    //
    // Mutex:  must be holding mMutex when called
    // <FS:Ansariel> [UDP Assets]
    //void setGetMeshCap(const std::string & get_mesh);
    void setGetMeshCap(const std::string & get_mesh, const std::string & legacy_get_mesh1, const std::string & legacy_get_mesh2, int legacy_pref_version);
    // </FS:Ansariel> [UDP Assets]

    // Mutex:  acquires mMutex
    // <FS:Ansariel> [UDP Assets]
    //void constructUrl(LLUUID mesh_id, std::string * url);
    void constructUrl(LLUUID mesh_id, std::string * url, int * legacy_version);
    // </FS:Ansariel> [UDP Assets]

    // <FS:Ansariel> DAE export
    LLUUID getCreatorFromHeader(const LLUUID& mesh_id);

private:
    // Issue a GET request to a URL with 'Range' header using
    // the correct policy class and other attributes.  If an invalid
    // handle is returned, the request failed and caller must retry
    // or dispose of handler.
    //
    // Threads:  Repo thread only
    // <FS:Ansariel> [UDP Assets]
    //LLCore::HttpHandle getByteRange(const std::string & url,
    LLCore::HttpHandle getByteRange(const std::string & url, int legacy_cap_version,
    // </FS:Ansariel> [UDP Assets]
                                    size_t offset, size_t len,
                                    const LLCore::HttpHandler::ptr_t &handler);

    // Mutex: acquires mPendingMutex, mMutex and mHeaderMutex as needed
    void loadMeshLOD(const LLUUID &mesh_id, const LLVolumeParams& mesh_params, S32 lod);

    // Threads:  Repo thread only
    U8* getDiskCacheBuffer(S32 size);
    S32 mDiskCacheBufferSize = 0;
    U8* mDiskCacheBuffer = nullptr;
    bool mShuttingDown = false;
};


// Class whose instances represent a single upload-type request for
// meshes:  one fee query or one actual upload attempt.  Yes, it creates
// a unique thread for that single request.  As it is 1:1, it can also
// trivially serve as the HttpHandler object for request completion
// notifications.

class LLMeshUploadThread : public LLThread, public LLCore::HttpHandler
{
private:
    S32 mMeshUploadTimeOut ; //maximum time in seconds to execute an uploading request.

public:
    class DecompRequest : public LLPhysicsDecomp::Request
    {
    public:
        LLPointer<LLModel> mModel;
        LLPointer<LLModel> mBaseModel;

        LLMeshUploadThread* mThread;

        DecompRequest(LLModel* mdl, LLModel* base_model, LLMeshUploadThread* thread);

        S32 statusCallback(const char* status, S32 p1, S32 p2) { return 1; }
        void completed();
    };

    LLPointer<DecompRequest> mFinalDecomp;
    volatile bool   mPhysicsComplete;

    typedef std::map<LLPointer<LLModel>, std::vector<LLVector3> > hull_map;
    hull_map        mHullMap;

    typedef std::vector<LLModelInstance> instance_list;
    instance_list   mInstanceList;

    typedef std::map<LLPointer<LLModel>, instance_list> instance_map;
    instance_map    mInstance;

    LLMutex*        mMutex;
    S32             mPendingUploads;
    LLVector3       mOrigin;
    bool            mFinished;
    bool            mUploadTextures;
    bool            mUploadSkin;
    bool            mUploadJoints;
    bool            mLockScaleIfJointPosition;
    volatile bool   mDiscarded;

    LLHost          mHost;
    std::string     mWholeModelFeeCapability;
    std::string     mWholeModelUploadURL;

    LLMeshUploadThread(instance_list& data, LLVector3& scale, bool upload_textures,
                       bool upload_skin, bool upload_joints, bool lock_scale_if_joint_position,
                       const std::string & upload_url, bool do_upload = true,
                       LLHandle<LLWholeModelFeeObserver> fee_observer = (LLHandle<LLWholeModelFeeObserver>()),
                       LLHandle<LLWholeModelUploadObserver> upload_observer = (LLHandle<LLWholeModelUploadObserver>()));
    ~LLMeshUploadThread();

    bool finished() const { return mFinished; }
    virtual void run();
    void preStart();
    void discard() ;
    bool isDiscarded() const;

    void generateHulls();

    void doWholeModelUpload();
    void requestWholeModelFee();

    void wholeModelToLLSD(LLSD& dest, bool include_textures);

    void decomposeMeshMatrix(LLMatrix4& transformation,
                             LLVector3& result_pos,
                             LLQuaternion& result_rot,
                             LLVector3& result_scale);

    void setFeeObserverHandle(LLHandle<LLWholeModelFeeObserver> observer_handle) { mFeeObserverHandle = observer_handle; }
    void setUploadObserverHandle(LLHandle<LLWholeModelUploadObserver> observer_handle) { mUploadObserverHandle = observer_handle; }

    // Inherited from LLCore::HttpHandler
    virtual void onCompleted(LLCore::HttpHandle handle, LLCore::HttpResponse * response);

    static LLViewerFetchedTexture* FindViewerTexture(const LLImportMaterial& material);

private:
    LLHandle<LLWholeModelFeeObserver> mFeeObserverHandle;
    LLHandle<LLWholeModelUploadObserver> mUploadObserverHandle;

    bool mDoUpload; // if false only model data will be requested, otherwise the model will be uploaded
    LLSD mModelData;

    // llcorehttp library interface objects.
    LLCore::HttpStatus                  mHttpStatus;
    LLCore::HttpRequest *               mHttpRequest;
    LLCore::HttpOptions::ptr_t          mHttpOptions;
    LLCore::HttpHeaders::ptr_t          mHttpHeaders;
    LLCore::HttpRequest::policy_t       mHttpPolicyClass;
};

// Params related to streaming cost, render cost, and scene complexity tracking.
class LLMeshCostData
{
public:
    LLMeshCostData();

    bool init(const LLMeshHeader& header);

    // Size for given LOD
    S32 getSizeByLOD(S32 lod) const;

    // Sum of all LOD sizes.
    S32 getSizeTotal() const;

    // Estimated triangle counts for the given LOD.
    F32 getEstTrisByLOD(S32 lod) const;

    // Estimated triangle counts for the largest LOD. Typically this
    // is also the "high" LOD, but not necessarily.
    F32 getEstTrisMax() const;

    // Triangle count as computed by original streaming cost
    // formula. Triangles in each LOD are weighted based on how
    // frequently they will be seen.
    // This was called "unscaled_value" in the original getStreamingCost() functions.
    F32 getRadiusWeightedTris(F32 radius) const;

    // Triangle count used by triangle-based cost formula. Based on
    // triangles in highest LOD plus potentially partial charges for
    // lower LODs depending on complexity.
    F32 getEstTrisForStreamingCost() const;

    // Streaming cost. This should match the server-side calculation
    // for the corresponding volume.
    F32 getRadiusBasedStreamingCost(F32 radius) const;

    // New streaming cost formula, currently only used for animated objects.
    F32 getTriangleBasedStreamingCost() const;

private:
    // From the "size" field of the mesh header. LOD 0=lowest, 3=highest.
    std::array<S32,4> mSizeByLOD;

    // Estimated triangle counts derived from the LOD sizes. LOD 0=lowest, 3=highest.
    std::array<F32,4> mEstTrisByLOD;
};

class LLMeshRepository
{
public:

    //metrics
    static U32 sBytesReceived;
    static U32 sMeshRequestCount;               // Total request count, http or cached, all component types
    static U32 sHTTPRequestCount;               // Http GETs issued (not large)
    static U32 sHTTPLargeRequestCount;          // Http GETs issued for large requests
    static U32 sHTTPRetryCount;                 // Total request retries whether successful or failed
    static U32 sHTTPErrorCount;                 // Requests ending in error
    static U32 sLODPending;
    static U32 sLODProcessing;
    static U32 sCacheBytesRead;
    static std::atomic<U32> sCacheBytesWritten;
    static U32 sCacheBytesHeaders;
    static U32 sCacheBytesSkins;
    static U32 sCacheBytesDecomps;
    static U32 sCacheReads;
    static std::atomic<U32> sCacheWrites;
    static U32 sMaxLockHoldoffs;                // Maximum sequential locking failures

    static LLDeadmanTimer sQuiescentTimer;      // Time-to-complete-mesh-downloads after significant events

    // Estimated triangle count of the largest LOD
    F32 getEstTrianglesMax(LLUUID mesh_id);
    F32 getEstTrianglesStreamingCost(LLUUID mesh_id);
    F32 getStreamingCostLegacy(LLUUID mesh_id, F32 radius, S32* bytes = NULL, S32* visible_bytes = NULL, S32 detail = -1, F32 *unscaled_value = NULL);
    static F32 getStreamingCostLegacy(LLMeshHeader& header, F32 radius, S32* bytes = NULL, S32* visible_bytes = NULL, S32 detail = -1, F32 *unscaled_value = NULL);
    bool getCostData(LLUUID mesh_id, LLMeshCostData& data);
    // <FS:ND> Use a const ref, just to make sure no one modifies header and we can pass a copy.
    // bool getCostData(LLMeshHeader& header, LLMeshCostData& data);
    bool getCostData(const LLMeshHeader& header, LLMeshCostData& data);
    // </FS:ND>

    LLMeshRepository();

    void init();
    void shutdown();
    S32 update();

    void unregisterMesh(LLVOVolume* volume);
    //mesh management functions
    S32 loadMesh(LLVOVolume* volume, const LLVolumeParams& mesh_params, S32 new_lod = 0, S32 last_lod = -1);

    void notifyLoadedMeshes();
    void notifyMeshLoaded(const LLVolumeParams& mesh_params, LLVolume* volume, S32 lod);
    void notifyMeshUnavailable(const LLVolumeParams& mesh_params, S32 request_lod, S32 volume_lod);
    void notifySkinInfoReceived(LLMeshSkinInfo* info);
    void notifySkinInfoUnavailable(const LLUUID& info);
    void notifyDecompositionReceived(LLModel::Decomposition* info);

    S32 getActualMeshLOD(const LLVolumeParams& mesh_params, S32 lod);
    static S32 getActualMeshLOD(LLMeshHeader& header, S32 lod);
    const LLMeshSkinInfo* getSkinInfo(const LLUUID& mesh_id, LLVOVolume* requesting_obj = nullptr);
    LLModel::Decomposition* getDecomposition(const LLUUID& mesh_id);
    void fetchPhysicsShape(const LLUUID& mesh_id);
    bool hasPhysicsShape(const LLUUID& mesh_id);
    bool hasSkinInfo(const LLUUID& mesh_id);
    bool hasHeader(const LLUUID& mesh_id) const;

    void buildHull(const LLVolumeParams& params, S32 detail);
    void buildPhysicsMesh(LLModel::Decomposition& decomp);

    bool meshUploadEnabled();
    bool meshRezEnabled();

    void uploadModel(std::vector<LLModelInstance>& data, LLVector3& scale, bool upload_textures,
                     bool upload_skin, bool upload_joints, bool lock_scale_if_joint_position,
                     std::string upload_url, bool do_upload = true,
                     LLHandle<LLWholeModelFeeObserver> fee_observer= (LLHandle<LLWholeModelFeeObserver>()),
                     LLHandle<LLWholeModelUploadObserver> upload_observer = (LLHandle<LLWholeModelUploadObserver>()));

    S32 getMeshSize(const LLUUID& mesh_id, S32 lod) const;

    // Quiescent timer management, main thread only.
    static void metricsStart();
    static void metricsStop();
    static void metricsProgress(unsigned int count);
    static void metricsUpdate();

    typedef std::unordered_map<LLUUID, MeshLoadData> mesh_load_map;
    mesh_load_map mLoadingMeshes[4];

    // <FS:Ansariel> DAE export
    LLUUID getCreatorFromHeader(const LLUUID& mesh_id);

    typedef std::unordered_map<LLUUID, LLPointer<LLMeshSkinInfo>> skin_map;
    skin_map mSkinMap;

    typedef std::map<LLUUID, LLModel::Decomposition*> decomposition_map;
    decomposition_map mDecompositionMap;

    LLMutex*                    mMeshMutex;

    typedef std::vector <std::shared_ptr<PendingRequestBase> > pending_requests_vec;
    pending_requests_vec mPendingRequests;

    //list of mesh ids awaiting skin info
    typedef std::unordered_map<LLUUID, MeshLoadData > skin_load_map;
    skin_load_map mLoadingSkins;

    //list of mesh ids awaiting decompositions
    std::unordered_set<LLUUID> mLoadingDecompositions;

    //list of mesh ids that need to send decomposition fetch requests
    std::queue<LLUUID> mPendingDecompositionRequests;

    //list of mesh ids awaiting physics shapes
    std::unordered_set<LLUUID> mLoadingPhysicsShapes;

    //list of mesh ids that need to send physics shape fetch requests
    std::queue<LLUUID> mPendingPhysicsShapeRequests;

    U32 mMeshThreadCount;

    LLMeshRepoThread* mThread;
    std::vector<LLMeshUploadThread*> mUploads;
    std::vector<LLMeshUploadThread*> mUploadWaitList;

    LLPhysicsDecomp* mDecompThread;

    LLFrameTimer     mSkinInfoCullTimer;

    class inventory_data
    {
    public:
        LLSD mPostData;
        LLSD mResponse;

        inventory_data(const LLSD& data, const LLSD& content)
            : mPostData(data), mResponse(content)
        {
        }
    };

    std::queue<inventory_data> mInventoryQ;

    std::queue<LLSD> mUploadErrorQ;

    void uploadError(LLSD& args);
    void updateInventory(inventory_data data);

    // <FS:Ansariel> [UDP Assets]
    int mLegacyGetMeshVersion;      // Shadows value in LLMeshRepoThread
};

extern LLMeshRepository gMeshRepo;

const F32 ANIMATED_OBJECT_BASE_COST = 15.0f;
const F32 ANIMATED_OBJECT_COST_PER_KTRI = 1.5f;

#endif

