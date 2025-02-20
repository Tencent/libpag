#include "AEGP_Utils.h"


A_Err GetNewFirstLayerInFirstComp(
								  SPBasicSuite		*sP,
								  AEGP_LayerH			*first_layerPH)
{
	A_Err err = A_Err_NONE;
	
	AEGP_ItemH			itemH					= NULL;
	AEGP_ItemType		type					= AEGP_ItemType_NONE;
	AEGP_CompH			compH					= NULL;
	AEGP_ProjectH		projH					= NULL;
	A_long				num_projectsL			= 0,
	num_layersL				= 0;
	
	AEGP_SuiteHandler	suites(sP);
	
	ERR(suites.ProjSuite5()->AEGP_GetProjectByIndex(0, &projH));
	ERR(suites.ItemSuite8()->AEGP_GetFirstProjItem(projH, &itemH));
	ERR(suites.ItemSuite6()->AEGP_GetItemType(itemH, &type));
	
	while ((itemH != NULL) && (type != AEGP_ItemType_COMP)){
		ERR(suites.ItemSuite6()->AEGP_GetNextProjItem(projH, itemH, &itemH));
		ERR(suites.ItemSuite6()->AEGP_GetItemType(itemH, &type));
	}
	if (!err && (type == AEGP_ItemType_COMP)){
		err = suites.CompSuite4()->AEGP_GetCompFromItem(itemH, &compH);
	}
	if (!err && compH) {
		err = suites.LayerSuite5()->AEGP_GetCompNumLayers(compH, &num_layersL);
	}
	if (!err && num_layersL){
		err = suites.LayerSuite5()->AEGP_GetCompLayerByIndex(compH, 0, first_layerPH);
	}
	return err;
}