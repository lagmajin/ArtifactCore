module;
export module Core.ArtifactFoundation;

// All custom collection types. No std dependency. No Qt dependency.
// import Core.ArtifactFoundation;

export import Core.ArtifactArray;
export import Core.ArtifactDict;
export import Core.ArtifactExpected;
export import Core.ArtifactFunctionRef;
export import Core.ArtifactString;
export import Core.ArtifactPtr;     // Ptr<T>, Ref<T>, Owned<T>, WeakPtr<T>
export import Core.ArtifactOptional;
export import Core.ArtifactAtomic;
export import Core.ArtifactHashMap;
export import Core.ArtifactSpan;
export import Core.ArtifactVariant;
export import Core.ArtifactThread;  // Mutex, Lock, Cond, Thread
export import Core.ArtifactCallback; // Callback<Signature>, Action
export import Core.ArtifactUtility; // artifactMove/Forward/Exchange/BitCast/Cmp*
export import Core.ArtifactMath;    // artifactMax/Min/Clamp/Abs/IsFinite/...
export import Core.ArtifactAlgorithms; // artifactSort/Find/Fill/LowerBound/...
export import Core.ArtifactSaturation;
export import Core.ArtifactTuple;   // Tuple<Ts...>, artifactGet<I>, artifactMakeTuple
export import Core.ArtifactChrono;  // Duration, SteadyClock, Stopwatch
