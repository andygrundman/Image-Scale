#define BLUR 1.0

typedef struct _FilterInfo {
  float (*function)(const float, const float),
  support;
} FilterInfo;

typedef struct _FilterInfoFixed {
  fixed_t (*function)(const fixed_t, const fixed_t),
  support;
} FilterInfoFixed;

typedef enum {
  UndefinedFilter,
  PointFilter,
  BoxFilter,
  TriangleFilter,
  HermiteFilter,
  HanningFilter,
  HammingFilter,
  BlackmanFilter,
  GaussianFilter,
  QuadraticFilter,
  CubicFilter,
  CatromFilter,
  MitchellFilter,
  LanczosFilter,
  BesselFilter,
  SincFilter
} FilterTypes;

typedef struct _ContributionInfo {
  float weight;
  int pixel;
} ContributionInfo;

typedef struct _ContributionInfoFixed {
  fixed_t weight;
  int pixel;
} ContributionInfoFixed;

typedef struct _ImageInfo {
  int32_t rows;
  int32_t columns;
  pix *buf;
} ImageInfo;
