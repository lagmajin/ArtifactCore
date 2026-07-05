module;
export module Core.ArtifactFoundation;

// All custom collection types. No std dependency. No Qt dependency.
// import Core.ArtifactFoundation;

export import Core.ArtifactArray;
export import Core.ArtifactDict;
export import Core.ArtifactString;
export import Core.ArtifactPtr;     // Ptr<T>, Ref<T>, Owned<T>, WeakPtr<T>
export import Core.ArtifactOptional;
export import Core.ArtifactAtomic;
export import Core.ArtifactHashMap;
export import Core.ArtifactSpan;
export import Core.ArtifactVariant;
export import Core.ArtifactThread;  // Mutex, Lock, Cond, Thread
export import Core.ArtifactCallback; // Callback<Signature>, Action
