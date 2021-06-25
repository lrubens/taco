#include "legion.h"
#include "taco_mapper.h"
#include "legion_utils.h"

using namespace Legion;

typedef double valType;

// Defined by the generated TACO code.
void registerTacoTasks();
LogicalPartition partitionLegionA(Context ctx, Runtime* runtime, LogicalRegion A, int32_t gridX, int32_t gridY);
LogicalPartition partitionLegionB(Context ctx, Runtime* runtime, LogicalRegion B, int32_t gridX, int32_t gridY);
LogicalPartition placeLegionA(Context ctx, Runtime* runtime, LogicalRegion A, int32_t gridX, int32_t gridY);
LogicalPartition placeLegionB(Context ctx, Runtime* runtime, LogicalRegion B, int32_t gridX, int32_t gridY);
LogicalPartition placeLegionC(Context ctx, Runtime* runtime, LogicalRegion C, int32_t gridX, int32_t gridY);
void computeLegion(Context ctx, Runtime* runtime, LogicalRegion A, LogicalRegion B, LogicalRegion C, LogicalPartition BPartition);

void top_level_task(const Task* task, const std::vector<PhysicalRegion>& regions, Context ctx, Runtime* runtime) {
  // Create the regions.
  auto args = runtime->get_input_args();
  int n = -1;
  int gx = -1;
  int gy = -1;
  // Parse input args.
  for (int i = 1; i < args.argc; i++) {
    if (strcmp(args.argv[i], "-n") == 0) {
      n = atoi(args.argv[++i]);
      continue;
    }
    if (strcmp(args.argv[i], "-gx") == 0) {
      gx = atoi(args.argv[++i]);
      continue;
    }
    if (strcmp(args.argv[i], "-gy") == 0) {
      gy = atoi(args.argv[++i]);
      continue;
    }
  }
  if (n == -1) {
    std::cout << "Please provide an input matrix size with -n." << std::endl;
    return;
  }
  if (gx == -1) {
    std::cout << "Please provide a grid x size with -gx." << std::endl;
    return;
  }
  if (gy == -1) {
    std::cout << "Please provide a gris y size with -gy." << std::endl;
    return;
  }

  auto fspace = runtime->create_field_space(ctx);
  allocate_tensor_fields<valType>(ctx, runtime, fspace);

  auto aISpace = runtime->create_index_space(ctx, Rect<2>({0, 0}, {n - 1, n - 1}));
  auto bISpace = runtime->create_index_space(ctx, Rect<3>({0, 0, 0}, {n - 1, n - 1, n - 1}));
  auto cISpace = runtime->create_index_space(ctx, Rect<1>({0, n - 1}));
  auto A = runtime->create_logical_region(ctx, aISpace, fspace); runtime->attach_name(A, "A");
  auto B = runtime->create_logical_region(ctx, bISpace, fspace); runtime->attach_name(B, "B");
  auto C = runtime->create_logical_region(ctx, cISpace, fspace); runtime->attach_name(C, "C");

  // Create initial partitions.
  auto aPart = partitionLegionA(ctx, runtime, A, gx, gy);
  auto bPart = partitionLegionB(ctx, runtime, B, gx, gy);

  // We don't need to fill the large tensor in the loop.
  tacoFill<valType>(ctx, runtime, B, bPart, 1);
  tacoFill<valType>(ctx, runtime, C, 1);
  for (int i = 0; i < 10; i++) {
    tacoFill<valType>(ctx, runtime, A, aPart, 0);

    // Place the tensors.
    placeLegionA(ctx, runtime, A, gx, gy);
    auto part = placeLegionB(ctx, runtime, B, gx, gy);
    placeLegionC(ctx, runtime, C, gx, gy);

    // Compute.
    benchmark(ctx, runtime, [&]() { computeLegion(ctx, runtime, A, B, C, part); });
  }

  tacoValidate<valType>(ctx, runtime, A, aPart, valType(n));
}

TACO_MAIN(valType)