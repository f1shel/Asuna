#include "loader.h"
#include "utils.h"

#include <nvmath/nvmath.h>

static float dielectricReflectance(float eta, float cosThetaI,
                                   float& cosThetaT) {
  if (cosThetaI < 0.0f) {
    eta = 1.0f / eta;
    cosThetaI = -cosThetaI;
  }
  float sinThetaTSq = eta * eta * (1.0f - cosThetaI * cosThetaI);
  if (sinThetaTSq > 1.0f) {
    cosThetaT = 0.0f;
    return 1.0f;
  }
  cosThetaT = std::sqrt(std::max(1.0f - sinThetaTSq, 0.0f));

  float Rs = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);
  float Rp = (eta * cosThetaT - cosThetaI) / (eta * cosThetaT + cosThetaI);

  return (Rs * Rs + Rp * Rp) * 0.5f;
}

static float computeDiffuseFresnel(float ior, const int sampleCount) {
  double diffuseFresnel = 0.0;
  float _, fb = dielectricReflectance(ior, 0.0f, _);
  for (int i = 1; i <= sampleCount; ++i) {
    float cosThetaSq = float(i) / sampleCount;
    float _, fa = dielectricReflectance(
                 ior, std::min(std::sqrt(cosThetaSq), 1.0f), _);
    diffuseFresnel += double(fa + fb) * (0.5 / sampleCount);
    fb = fa;
  }

  return float(diffuseFresnel);
}

// clang-format off
static void ParseBrdfLambertian(Scene* m_pScene, const nlohmann::json& materialJson, GpuMaterial& material);
static void ParseBrdfPbrMetalnessRoughness(Scene* m_pScene, const nlohmann::json& materialJson, GpuMaterial& material);
static void ParseBrdfEmissive(Scene* m_pScene, const nlohmann::json& materialJson, GpuMaterial& material);
static void ParseBrdfKang18(Scene* m_pScene, const nlohmann::json& materialJson, GpuMaterial& material);
static void ParseBsdfDielectric(Scene* m_pScene, const nlohmann::json& materialJson, GpuMaterial& material);
static void ParseBrdfPlastic(Scene* m_pScene, const nlohmann::json& materialJson, GpuMaterial& material);
static void ParseBrdfRoughPlastic(Scene* m_pScene, const nlohmann::json& materialJson, GpuMaterial& material);
static void ParseBrdfMirror(Scene* m_pScene, const nlohmann::json& materialJson, GpuMaterial& material);
static void ParseBrdfConductor(Scene* m_pScene, const nlohmann::json& materialJson, GpuMaterial& material);
static void ParseBrdfRoughConductor(Scene* m_pScene, const nlohmann::json& materialJson, GpuMaterial& material);
static void ParseBrdfDisney(Scene* m_pScene, const nlohmann::json& materialJson, GpuMaterial& material);
static void ParseBrdfPhong(Scene* m_pScene, const nlohmann::json& materialJson, GpuMaterial& material);
// clang-format on

void Loader::addMaterial(const nlohmann::json& materialJson) {
  JsonCheckKeys(materialJson, {"type", "name"});
  std::string materialName = materialJson["name"];
  Material mat;
  GpuMaterial& material = mat.getMaterial();
  if (materialJson["type"] == "brdf_lambertian") {
    material.type = MaterialTypeBrdfLambertian;
    ParseBrdfLambertian(m_pScene, materialJson, material);
  } else if (materialJson["type"] == "brdf_pbr_metalness_roughness") {
    material.type = MaterialTypeBrdfPbrMetalnessRoughness;
    ParseBrdfPbrMetalnessRoughness(m_pScene, materialJson, material);
  } else if (materialJson["type"] == "brdf_emissive") {
    material.type = MaterialTypeBrdfEmissive;
    ParseBrdfEmissive(m_pScene, materialJson, material);
  } else if (materialJson["type"] == "brdf_kang18") {
    material.type = MaterialTypeBrdfKang18;
    ParseBrdfKang18(m_pScene, materialJson, material);
  } else if (materialJson["type"] == "bsdf_dielectric") {
    material.type = MaterialTypeBsdfDielectric;
    ParseBsdfDielectric(m_pScene, materialJson, material);
  } else if (materialJson["type"] == "brdf_plastic") {
    material.type = MaterialTypeBrdfPlastic;
    ParseBrdfPlastic(m_pScene, materialJson, material);
  } else if (materialJson["type"] == "brdf_rough_plastic") {
    material.type = MaterialTypeBrdfRoughPlastic;
    ParseBrdfRoughPlastic(m_pScene, materialJson, material);
  } else if (materialJson["type"] == "brdf_mirror") {
    material.type = MaterialTypeBrdfMirror;
    ParseBrdfMirror(m_pScene, materialJson, material);
  } else if (materialJson["type"] == "brdf_conductor") {
    material.type = MaterialTypeBrdfConductor;
    ParseBrdfConductor(m_pScene, materialJson, material);
  } else if (materialJson["type"] == "brdf_rough_conductor") {
    material.type = MaterialTypeBrdfRoughConductor;
    ParseBrdfRoughConductor(m_pScene, materialJson, material);
  } else if (materialJson["type"] == "brdf_disney") {
    material.type = MaterialTypeBrdfDisney;
    ParseBrdfDisney(m_pScene, materialJson, material);
  } else if (materialJson["type"] == "brdf_phong") {
    material.type = MaterialTypeBrdfPhong;
    ParseBrdfPhong(m_pScene, materialJson, material);
  } else {
    LOG_ERROR("{}: unrecognized material type [{}]", "Loader",
              materialJson["type"]);
    exit(1);
  }

  m_pScene->addMaterial(materialName, material);
}

static void ParseBrdfLambertian(Scene* m_pScene,
                                const nlohmann::json& materialJson,
                                GpuMaterial& material) {
  if (materialJson.contains("diffuse_reflectance"))
    material.diffuse = Json2Vec3(materialJson["diffuse_reflectance"]);
  if (materialJson.contains("diffuse_texture"))
    material.diffuseTextureId =
        m_pScene->getTextureId(materialJson["diffuse_texture"]);
  if (materialJson.contains("normal_texture"))
    material.normalTextureId =
        m_pScene->getTextureId(materialJson["normal_texture"]);
}
static void ParseBrdfPbrMetalnessRoughness(Scene* m_pScene,
                                           const nlohmann::json& materialJson,
                                           GpuMaterial& material) {
  if (materialJson.contains("normal_texture"))
    material.normalTextureId =
        m_pScene->getTextureId(materialJson["normal_texture"]);
  if (materialJson.contains("diffuse_reflectance"))
    material.diffuse = Json2Vec3(materialJson["diffuse_reflectance"]);
  if (materialJson.contains("diffuse_texture"))
    material.diffuseTextureId =
        m_pScene->getTextureId(materialJson["diffuse_texture"]);
  if (materialJson.contains("metalness"))
    material.metalness = materialJson["metalness"];
  if (materialJson.contains("metalness_texture"))
    material.metalnessTextureId =
        m_pScene->getTextureId(materialJson["metalness_texture"]);
  if (materialJson.contains("roughness"))
    material.roughness = materialJson["roughness"];
  if (materialJson.contains("roughness_texture"))
    material.roughnessTextureId =
        m_pScene->getTextureId(materialJson["roughness_texture"]);
  // Used as opacity
  material.specular = 0.f;
  if (materialJson.count("opacity_texture")) {
    material.opacityTextureId =
        m_pScene->getTextureId(materialJson["opacity_texture"]);
  }
}

static void ParseBrdfEmissive(Scene* m_pScene,
                              const nlohmann::json& materialJson,
                              GpuMaterial& material) {
  if (materialJson.contains("radiance"))
    material.radiance = Json2Vec3(materialJson["radiance"]);
  if (materialJson.contains("radiance_factor"))
    material.radianceFactor = Json2Vec3(materialJson["radiance_factor"]);
  if (materialJson.contains("radiance_texture"))
    material.radianceTextureId =
        m_pScene->getTextureId(materialJson["radiance_texture"]);
}
static void ParseBrdfKang18(Scene* m_pScene, const nlohmann::json& materialJson,
                            GpuMaterial& material) {
  //JsonCheckKeys(materialJson, {"normal_texture", "tangent_texture"});
  if (materialJson.contains("normal_texture"))
    material.normalTextureId =
      m_pScene->getTextureId(materialJson["normal_texture"]);
  if (materialJson.contains("tangent_texture"))
    material.tangentTextureId =
      m_pScene->getTextureId(materialJson["tangent_texture"]);
  if (materialJson.contains("diffuse_texture"))
    material.diffuseTextureId =
        m_pScene->getTextureId(materialJson["diffuse_texture"]);
  else
    material.diffuse = Json2Vec3(materialJson["diffuse_reflectance"]);
  if (materialJson.contains("specular_texture"))
    material.metalnessTextureId =
        m_pScene->getTextureId(materialJson["specular_texture"]);
  else
    material.rhoSpec = Json2Vec3(materialJson["specular_reflectance"]);
  if (materialJson.contains("alpha_texture"))
    material.roughnessTextureId =
        m_pScene->getTextureId(materialJson["alpha_texture"]);
  else
    material.anisoAlpha = Json2Vec2(materialJson["alpha"]);

  // Used as opacity
  material.metalness = 0.f;
  if (materialJson.count("opacity_texture")) {
    material.opacityTextureId =
        m_pScene->getTextureId(materialJson["opacity_texture"]);
  }
}

static void ParseBsdfDielectric(Scene* m_pScene,
                                const nlohmann::json& materialJson,
                                GpuMaterial& material) {
  if (materialJson.contains("normal_texture"))
    material.normalTextureId =
        m_pScene->getTextureId(materialJson["normal_texture"]);
  if (materialJson.contains("ior")) material.ior = materialJson["ior"];
}
static void ParseBrdfPlastic(Scene* m_pScene,
                             const nlohmann::json& materialJson,
                             GpuMaterial& material) {
  if (materialJson.contains("ior")) material.ior = materialJson["ior"];
  if (materialJson.contains("diffuse_reflectance"))
    material.diffuse = Json2Vec3(materialJson["diffuse_reflectance"]);
  if (materialJson.contains("diffuse_texture"))
    material.diffuseTextureId =
        m_pScene->getTextureId(materialJson["diffuse_texture"]);
  if (materialJson.contains("normal_texture"))
    material.normalTextureId =
        m_pScene->getTextureId(materialJson["normal_texture"]);
  // fresnel diffuse reflectance, AKA fdrInt
  material.radiance.x = computeDiffuseFresnel(material.ior, 1000);
}
static void ParseBrdfRoughPlastic(Scene* m_pScene,
                                  const nlohmann::json& materialJson,
                                  GpuMaterial& material) {
  if (materialJson.contains("ior")) material.ior = materialJson["ior"];
  if (materialJson.contains("diffuse_reflectance"))
    material.diffuse = Json2Vec3(materialJson["diffuse_reflectance"]);
  if (materialJson.contains("diffuse_texture"))
    material.diffuseTextureId =
        m_pScene->getTextureId(materialJson["diffuse_texture"]);
  if (materialJson.contains("normal_texture"))
    material.normalTextureId =
        m_pScene->getTextureId(materialJson["normal_texture"]);
  if (materialJson.contains("alpha"))
    material.anisoAlpha = Json2Vec2(materialJson["alpha"]);
  if (materialJson.contains("alpha_texture"))
    material.roughnessTextureId =
        m_pScene->getTextureId(materialJson["alpha_texture"]);
  // fresnel diffuse reflectance, AKA fdrInt
  material.radiance.x = computeDiffuseFresnel(material.ior, 1000);
}

static void ParseBrdfMirror(Scene* m_pScene, const nlohmann::json& materialJson,
                            GpuMaterial& material) {
  if (materialJson.contains("diffuse_reflectance"))
    material.diffuse = Json2Vec3(materialJson["diffuse_reflectance"]);
  if (materialJson.contains("diffuse_texture"))
    material.diffuseTextureId =
        m_pScene->getTextureId(materialJson["diffuse_texture"]);
  if (materialJson.contains("normal_texture"))
    material.normalTextureId =
        m_pScene->getTextureId(materialJson["normal_texture"]);
}

// clang-format off
struct ComplexIor {
  string name;
  vec3 eta, k;
};
static const ComplexIor complexIorList[] = {
  {"a-C",    vec3(2.9440999183f, 2.2271502925f, 1.9681668794f), vec3(0.8874329109f, 0.7993216383f, 0.8152862927f)},
  {"Ag",     vec3(0.1552646489f, 0.1167232965f, 0.1383806959f), vec3(4.8283433224f, 3.1222459278f, 2.1469504455f)},
  {"Al",     vec3(1.6574599595f, 0.8803689579f, 0.5212287346f), vec3(9.2238691996f, 6.2695232477f, 4.8370012281f)},
  {"AlAs",   vec3(3.6051023902f, 3.2329365777f, 2.2175611545f), vec3(0.0006670247f, -0.0004999400f, 0.0074261204f)},
  {"AlSb",   vec3(-0.0485225705f, 4.1427547893f, 4.6697691348f), vec3(-0.0363741915f, 0.0937665154f, 1.3007390124f)},
  {"Au",     vec3(0.1431189557f, 0.3749570432f, 1.4424785571f), vec3(3.9831604247f, 2.3857207478f, 1.6032152899f)},
  {"Be",     vec3(4.1850592788f, 3.1850604423f, 2.7840913457f), vec3(3.8354398268f, 3.0101260162f, 2.8690088743f)},
  {"Cr",     vec3(4.3696828663f, 2.9167024892f, 1.6547005413f), vec3(5.2064337956f, 4.2313645277f, 3.7549467933f)},
  {"CsI",    vec3(2.1449030413f, 1.7023164587f, 1.6624194173f), vec3(0.0000000000f, 0.0000000000f, 0.0000000000f)},
  {"Cu",     vec3(0.2004376970f, 0.9240334304f, 1.1022119527f), vec3(3.9129485033f, 2.4528477015f, 2.1421879552f)},
  {"Cu2O",   vec3(3.5492833755f, 2.9520622449f, 2.7369202137f), vec3(0.1132179294f, 0.1946659670f, 0.6001681264f)},
  {"CuO",    vec3(3.2453822204f, 2.4496293965f, 2.1974114493f), vec3(0.5202739621f, 0.5707372756f, 0.7172250613f)},
  {"d-C",    vec3(2.7112524747f, 2.3185812849f, 2.2288565009f), vec3(0.0000000000f, 0.0000000000f, 0.0000000000f)},
  {"Hg",     vec3(2.3989314904f, 1.4400254917f, 0.9095512090f), vec3(6.3276269444f, 4.3719414152f, 3.4217899270f)},
  {"HgTe",   vec3(4.7795267752f, 3.2309984581f, 2.6600252401f), vec3(1.6319827058f, 1.5808189339f, 1.7295753852f)},
  {"Ir",     vec3(3.0864098394f, 2.0821938440f, 1.6178866805f), vec3(5.5921510077f, 4.0671757150f, 3.2672611269f)},
  {"K",      vec3(0.0640493070f, 0.0464100621f, 0.0381842017f), vec3(2.1042155920f, 1.3489364357f, 0.9132113889f)},
  {"Li",     vec3(0.2657871942f, 0.1956102432f, 0.2209198538f), vec3(3.5401743407f, 2.3111306542f, 1.6685930000f)},
  {"MgO",    vec3(2.0895885542f, 1.6507224525f, 1.5948759692f), vec3(0.0000000000f, -0.0000000000f, 0.0000000000f)},
  {"Mo",     vec3(4.4837010280f, 3.5254578255f, 2.7760769438f), vec3(4.1111307988f, 3.4208716252f, 3.1506031404f)},
  {"Na",     vec3(0.0602665320f, 0.0561412435f, 0.0619909494f), vec3(3.1792906496f, 2.1124800781f, 1.5790940266f)},
  {"Nb",     vec3(3.4201353595f, 2.7901921379f, 2.3955856658f), vec3(3.4413817900f, 2.7376437930f, 2.5799132708f)},
  {"Ni",     vec3(2.3672753521f, 1.6633583302f, 1.4670554172f), vec3(4.4988329911f, 3.0501643957f, 2.3454274399f)},
  {"Rh",     vec3(2.5857954933f, 1.8601866068f, 1.5544279524f), vec3(6.7822927110f, 4.7029501026f, 3.9760892461f)},
  {"Se-e",   vec3(5.7242724833f, 4.1653992967f, 4.0816099264f), vec3(0.8713747439f, 1.1052845009f, 1.5647788766f)},
  {"Se",     vec3(4.0592611085f, 2.8426947380f, 2.8207582835f), vec3(0.7543791750f, 0.6385150558f, 0.5215872029f)},
  {"SiC",    vec3(3.1723450205f, 2.5259677964f, 2.4793623897f), vec3(0.0000007284f, -0.0000006859f, 0.0000100150f)},
  {"SnTe",   vec3(4.5251865890f, 1.9811525984f, 1.2816819226f), vec3(0.0000000000f, 0.0000000000f, 0.0000000000f)},
  {"Ta",     vec3(2.0625846607f, 2.3930915569f, 2.6280684948f), vec3(2.4080467973f, 1.7413705864f, 1.9470377016f)},
  {"Te-e",   vec3(7.5090397678f, 4.2964603080f, 2.3698732430f), vec3(5.5842076830f, 4.9476231084f, 3.9975145063f)},
  {"Te",     vec3(7.3908396088f, 4.4821028985f, 2.6370708478f), vec3(3.2561412892f, 3.5273908133f, 3.2921683116f)},
  {"ThF4",   vec3(1.8307187117f, 1.4422274283f, 1.3876488528f), vec3(0.0000000000f, 0.0000000000f, 0.0000000000f)},
  {"TiC",    vec3(3.7004673762f, 2.8374356509f, 2.5823030278f), vec3(3.2656905818f, 2.3515586388f, 2.1727857800f)},
  {"TiN",    vec3(1.6484691607f, 1.1504482522f, 1.3797795097f), vec3(3.3684596226f, 1.9434888540f, 1.1020123347f)},
  {"TiO2-e", vec3(3.1065574823f, 2.5131551146f, 2.5823844157f), vec3(0.0000289537f, -0.0000251484f, 0.0001775555f)},
  {"TiO2",   vec3(3.4566203131f, 2.8017076558f, 2.9051485020f), vec3(0.0001026662f, -0.0000897534f, 0.0006356902f)},
  {"VC",     vec3(3.6575665991f, 2.7527298065f, 2.5326814570f), vec3(3.0683516659f, 2.1986687713f, 1.9631816252f)},
  {"VN",     vec3(2.8656011588f, 2.1191817791f, 1.9400767149f), vec3(3.0323264950f, 2.0561075580f, 1.6162930914f)},
  {"V",      vec3(4.2775126218f, 3.5131538236f, 2.7611257461f), vec3(3.4911844504f, 2.8893580874f, 3.1116965117f)},
  {"W",      vec3(4.3707029924f, 3.3002972445f, 2.9982666528f), vec3(3.5006778591f, 2.6048652781f, 2.2731930614f)},
};
static const int complexIorCount = 40;
// clang-format on

bool complexIorListLookup(const std::string& name, vec3& eta, vec3& k) {
  for (int i = 0; i < complexIorCount; ++i) {
    if (complexIorList[i].name == name) {
      eta = complexIorList[i].eta;
      k = complexIorList[i].k;
      return true;
    }
  }
  return false;
}

static void ParseBrdfConductor(Scene* m_pScene,
                               const nlohmann::json& materialJson,
                               GpuMaterial& material) {
  string materialName = "Cu";
  if (materialJson.contains("material"))
    materialName = materialJson["material"];
  bool found = complexIorListLookup(materialName, material.radiance,
                                    material.radianceFactor);
  if (!found) {
    LOG_ERROR("{}: unrecognized material name in brdf_conductor [{}]", "Loader",
              materialName);
    exit(1);
  }

  if (materialJson.contains("diffuse_reflectance"))
    material.diffuse = Json2Vec3(materialJson["diffuse_reflectance"]);
  if (materialJson.contains("diffuse_texture"))
    material.diffuseTextureId =
        m_pScene->getTextureId(materialJson["diffuse_texture"]);
  if (materialJson.contains("normal_texture"))
    material.normalTextureId =
        m_pScene->getTextureId(materialJson["normal_texture"]);
}

static void ParseBrdfRoughConductor(Scene* m_pScene,
                                    const nlohmann::json& materialJson,
                                    GpuMaterial& material) {
  string materialName = "Cu";
  if (materialJson.contains("material"))
    materialName = materialJson["material"];
  bool found = complexIorListLookup(materialName, material.radiance,
                                    material.radianceFactor);
  if (!found) {
    LOG_ERROR("{}: unrecognized material name in brdf_conductor [{}]", "Loader",
              materialName);
    exit(1);
  }

  if (materialJson.contains("diffuse_reflectance"))
    material.diffuse = Json2Vec3(materialJson["diffuse_reflectance"]);
  if (materialJson.contains("diffuse_texture"))
    material.diffuseTextureId =
        m_pScene->getTextureId(materialJson["diffuse_texture"]);
  if (materialJson.contains("normal_texture"))
    material.normalTextureId =
        m_pScene->getTextureId(materialJson["normal_texture"]);
  if (materialJson.contains("alpha"))
    material.anisoAlpha = Json2Vec2(materialJson["alpha"]);
  if (materialJson.contains("alpha_texture"))
    material.roughnessTextureId =
        m_pScene->getTextureId(materialJson["alpha_texture"]);
}

static void ParseBrdfDisney(Scene* m_pScene, const nlohmann::json& materialJson,
                            GpuMaterial& material) {
  if (materialJson.contains("normal_texture"))
    material.normalTextureId =
        m_pScene->getTextureId(materialJson["normal_texture"]);
  if (materialJson.contains("diffuse_reflectance"))
    material.diffuse = Json2Vec3(materialJson["diffuse_reflectance"]);
  if (materialJson.contains("diffuse_texture"))
    material.diffuseTextureId =
        m_pScene->getTextureId(materialJson["diffuse_texture"]);
  if (materialJson.contains("metallic"))
    material.metalness = materialJson["metallic"];
  if (materialJson.contains("metallic_texture"))
    material.metalnessTextureId =
        m_pScene->getTextureId(materialJson["metallic_texture"]);
  if (materialJson.contains("roughness"))
    material.roughness = materialJson["roughness"];
  if (materialJson.contains("roughness_texture"))
    material.roughnessTextureId =
        m_pScene->getTextureId(materialJson["roughness_texture"]);

  // Used as opacity
  material.rhoSpec.x = 0.f;
  if (materialJson.count("opacity")) {
    material.rhoSpec.x = materialJson["opacity"];
  }
  if (materialJson.count("opacity_texture")) {
    material.opacityTextureId =
        m_pScene->getTextureId(materialJson["opacity_texture"]);
  }
}

void ParseBrdfPhong(Scene* m_pScene, const nlohmann::json& materialJson,
                    GpuMaterial& material) {
  if (materialJson.contains("normal_texture"))
    material.normalTextureId =
        m_pScene->getTextureId(materialJson["normal_texture"]);
  if (materialJson.contains("diffuse_reflectance"))
    material.diffuse = Json2Vec3(materialJson["diffuse_reflectance"]);
  if (materialJson.contains("diffuse_texture"))
    material.diffuseTextureId =
        m_pScene->getTextureId(materialJson["diffuse_texture"]);
  if (materialJson.contains("specular_reflectance"))
    material.rhoSpec = Json2Vec3(materialJson["specular_reflectance"]);
  if (materialJson.contains("shininess"))
    material.specular = materialJson["shininess"];
}
