#include <platform_override.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_fixup.h>

static int xiangshan_kmh_fdt_fixup(void *fdt, const struct fdt_match *match)
{
	fdt_reserved_memory_nomap_fixup(fdt);

	return 0;
}

static const struct fdt_match xiangshan_kmh_match[] = {
	{ .compatible = "xiangshan,nemu-board" },
	{ .compatible = "xiangshan,kunminghu" },
	{ },
};

const struct platform_override xiangshan_kmh = {
	.match_table = xiangshan_kmh_match,
	.fdt_fixup = xiangshan_kmh_fdt_fixup,
};
