#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include "cli_CommandLineInterface.h"

#include "cli_Constants.h"

#include "sml_Names.h"

#include "IgSKI_Agent.h"
#include "IgSKI_Kernel.h"
#include "IgSKI_DoNotTouch.h"

using namespace cli;
using namespace sml;

bool CommandLineInterface::ParseInternalSymbols(gSKI::IAgent* pAgent, std::vector<std::string>& argv) {
	unused(argv);
	return DoInternalSymbols(pAgent);
}

bool CommandLineInterface::DoInternalSymbols(gSKI::IAgent* pAgent) {

	// Need agent pointer for function calls
	if (!RequireAgent(pAgent)) return false;

	// Attain the evil back door of doom, even though we aren't the TgD
	gSKI::EvilBackDoor::ITgDWorkArounds* pKernelHack = m_pKernel->getWorkaroundObject();
		
	AddListenerAndDisableCallbacks(pAgent);
	pKernelHack->PrintInternalSymbols(pAgent);
	RemoveListenerAndEnableCallbacks(pAgent);

	if (!m_RawOutput) {
		AppendArgTag(sml_Names::kParamMessage, sml_Names::kTypeString, m_Result.c_str());
		m_Result.clear();
	}
	return true;
}

