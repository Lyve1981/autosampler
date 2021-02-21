#include "cli.h"

int main(int argc, char* argv[])
{
	asCli::Cli cli(argc, argv);

	return cli.run();
}
