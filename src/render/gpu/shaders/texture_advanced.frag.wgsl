// I couldn't be bothered with manually porting that entire shader so I used Tint

struct Constants {
    scRGB_output: f32,
    texture_type: f32,
    input_type: f32,
    color_scale: f32,
    texel_size: vec4<f32>,
    tonemap_method: f32,
    tonemap_factor1: f32,
    tonemap_factor2: f32,
    sdr_white_point: f32,
    Yoffset: vec4<f32>,
    Rcoeff: vec4<f32>,
    Gcoeff: vec4<f32>,
    Bcoeff: vec4<f32>,
}

@group(3u) @binding(0u) var<uniform> v: Constants;

@group(2u) @binding(0u) var texture0: texture_2d<f32>;

@group(2u) @binding(1u) var sampler0: sampler;

@group(2u) @binding(2u) var texture1: texture_2d<f32>;

@group(2u) @binding(3u) var sampler1: sampler;

@group(2u) @binding(4u) var texture2: texture_2d<f32>;

@group(2u) @binding(5u) var sampler2: sampler;

var<private> v_1: vec4<f32>;

fn main_inner(v_2: vec4<f32>, v_3: vec2<f32>) {
    var v_4: vec4<f32>;
    var v_5: vec3<f32>;
    var v_6: vec3<f32>;
    var v_7: vec2<f32>;
    var v_8: vec2<f32>;
    var v_9: vec4<f32>;
    var v_10: vec3<f32>;
    var v_11: vec3<f32>;
    var v_12: vec3<f32>;
    var v_13: vec4<f32>;
    var v_14: vec3<f32>;
    var v_15: vec4<f32>;
    var v_16: vec3<f32>;
    var v_17: vec4<f32>;
    if (v.texture_type == 0.0f) {
        v_9 = vec4<f32>(1.0f);
    } else if (v.texture_type == 3.0f) {
        v_9 = textureSample(texture0, sampler0, v_3);
    } else if (v.texture_type == 4.0f) {
        let v_18 = clamp((fwidth(v_3) * v.texel_size.zw), vec2<f32>(0.00000999999974737875f), vec2<f32>(1.0f));
        let v_19 = ((v_3 * v.texel_size.zw) - (v_18 * 0.5f));
        let v_20 = smoothstep((vec2<f32>(1.0f) - v_18), vec2<f32>(1.0f), fract(v_19));
        let v_21 = (((floor(v_19) + vec2<f32>(0.5f)) + v_20) * v.texel_size.xy);
        v_9 = textureSampleGrad(texture0, sampler0, v_21, dpdx(v_3), dpdy(v_3));
    } else if (v.texture_type == 1.0f) {
        let v_22 = textureSample(texture0, sampler0, v_3);
        v_9 = vec4<f32>(v_22.x, v_22.y, v_22.z, 1.0f);
    } else if (v.texture_type == 2.0f) {
        let v_23 = clamp((fwidth(v_3) * v.texel_size.zw), vec2<f32>(0.00000999999974737875f), vec2<f32>(1.0f));
        let v_24 = ((v_3 * v.texel_size.zw) - (v_23 * 0.5f));
        let v_25 = smoothstep((vec2<f32>(1.0f) - v_23), vec2<f32>(1.0f), fract(v_24));
        let v_26 = (((floor(v_24) + vec2<f32>(0.5f)) + v_25) * v.texel_size.xy);
        let v_27 = textureSampleGrad(texture0, sampler0, v_26, dpdx(v_3), dpdy(v_3));
        v_9 = vec4<f32>(v_27.x, v_27.y, v_27.z, 1.0f);
    } else if (v.texture_type == 5.0f) {
        let v_28 = (textureSample(texture0, sampler0, v_3).x * 255.0f);
        v_9 = textureSample(texture1, sampler1, vec2<f32>(((v_28 + 0.5f) * 0.00390625f), 0.5f));
    } else if (v.texture_type == 6.0f) {
        let v_29 = ((v_3 * v.texel_size.zw) + vec2<f32>(0.5f));
        let v_30 = ((floor(v_29) - vec2<f32>(0.5f)) * v.texel_size.xy);
        let v_31 = ((floor(v_29) + vec2<f32>(0.5f)) * v.texel_size.xy);
        let v_32 = vec4<f32>(v_30.x, v_30.y, v_31.x, v_31.y);
        v_8 = fract(v_29);
        let v_33 = (textureSample(texture0, sampler0, v_32.xy).x * 255.0f);
        let v_34 = textureSample(texture1, sampler1, vec2<f32>(((v_33 + 0.5f) * 0.00390625f), 0.5f));
        let v_35 = (textureSample(texture0, sampler0, v_32.xw).x * 255.0f);
        let v_36 = textureSample(texture1, sampler1, vec2<f32>(((v_35 + 0.5f) * 0.00390625f), 0.5f));
        let v_37 = (textureSample(texture0, sampler0, v_32.zy).x * 255.0f);
        let v_38 = textureSample(texture1, sampler1, vec2<f32>(((v_37 + 0.5f) * 0.00390625f), 0.5f));
        let v_39 = (textureSample(texture0, sampler0, v_32.zw).x * 255.0f);
        let v_40 = textureSample(texture1, sampler1, vec2<f32>(((v_39 + 0.5f) * 0.00390625f), 0.5f));
        let v_41 = v_8.y;
        let v_42 = mix(v_34, v_36, vec4<f32>(v_41, v_41, v_41, v_41));
        let v_43 = v_8.y;
        let v_44 = mix(v_38, v_40, vec4<f32>(v_43, v_43, v_43, v_43));
        let v_45 = v_8.x;
        v_9 = mix(v_42, v_44, vec4<f32>(v_45, v_45, v_45, v_45));
    } else if (v.texture_type == 7.0f) {
        let v_46 = clamp((fwidth(v_3) * v.texel_size.zw), vec2<f32>(0.00000999999974737875f), vec2<f32>(1.0f));
        let v_47 = ((v_3 * v.texel_size.zw) - (v_46 * 0.5f));
        let v_48 = smoothstep((vec2<f32>(1.0f) - v_46), vec2<f32>(1.0f), fract(v_47));
        let v_49 = (((((floor(v_47) + vec2<f32>(0.5f)) + v_48) * v.texel_size.xy) * v.texel_size.zw) + vec2<f32>(0.5f));
        let v_50 = ((floor(v_49) - vec2<f32>(0.5f)) * v.texel_size.xy);
        let v_51 = ((floor(v_49) + vec2<f32>(0.5f)) * v.texel_size.xy);
        let v_52 = vec4<f32>(v_50.x, v_50.y, v_51.x, v_51.y);
        v_7 = fract(v_49);
        let v_53 = (textureSample(texture0, sampler0, v_52.xy).x * 255.0f);
        let v_54 = textureSample(texture1, sampler1, vec2<f32>(((v_53 + 0.5f) * 0.00390625f), 0.5f));
        let v_55 = (textureSample(texture0, sampler0, v_52.xw).x * 255.0f);
        let v_56 = textureSample(texture1, sampler1, vec2<f32>(((v_55 + 0.5f) * 0.00390625f), 0.5f));
        let v_57 = (textureSample(texture0, sampler0, v_52.zy).x * 255.0f);
        let v_58 = textureSample(texture1, sampler1, vec2<f32>(((v_57 + 0.5f) * 0.00390625f), 0.5f));
        let v_59 = (textureSample(texture0, sampler0, v_52.zw).x * 255.0f);
        let v_60 = textureSample(texture1, sampler1, vec2<f32>(((v_59 + 0.5f) * 0.00390625f), 0.5f));
        let v_61 = v_7.y;
        let v_62 = mix(v_54, v_56, vec4<f32>(v_61, v_61, v_61, v_61));
        let v_63 = v_7.y;
        let v_64 = mix(v_58, v_60, vec4<f32>(v_63, v_63, v_63, v_63));
        let v_65 = v_7.x;
        v_9 = mix(v_62, v_64, vec4<f32>(v_65, v_65, v_65, v_65));
    } else if (v.texture_type == 8.0f) {
        v_10.x = textureSample(texture0, sampler0, v_3).x;
        let v_66 = textureSample(texture1, sampler0, v_3);
        v_10.y = v_66.x;
        v_10.z = v_66.y;
        let v_67 = v.Yoffset.xyz;
        let v_68 = (v_10 + v_67);
        v_10 = v_68;
        v_9.x = dot(v_68, v.Rcoeff.xyz);
        v_9.y = dot(v_68, v.Gcoeff.xyz);
        v_9.z = dot(v_68, v.Bcoeff.xyz);
        v_9.w = 1.0f;
    } else if (v.texture_type == 9.0f) {
        v_11.x = textureSample(texture0, sampler0, v_3).x;
        let v_69 = textureSample(texture1, sampler0, v_3);
        v_11.y = v_69.y;
        v_11.z = v_69.x;
        let v_70 = v.Yoffset.xyz;
        let v_71 = (v_11 + v_70);
        v_11 = v_71;
        v_9.x = dot(v_71, v.Rcoeff.xyz);
        v_9.y = dot(v_71, v.Gcoeff.xyz);
        v_9.z = dot(v_71, v.Bcoeff.xyz);
        v_9.w = 1.0f;
    } else if (v.texture_type == 10.0f) {
        v_12.x = textureSample(texture0, sampler0, v_3).x;
        v_12.y = textureSample(texture1, sampler0, v_3).x;
        v_12.z = textureSample(texture2, sampler0, v_3).x;
        let v_72 = v.Yoffset.xyz;
        let v_73 = (v_12 + v_72);
        v_12 = v_73;
        v_9.x = dot(v_73, v.Rcoeff.xyz);
        v_9.y = dot(v_73, v.Gcoeff.xyz);
        v_9.z = dot(v_73, v.Bcoeff.xyz);
        v_9.w = 1.0f;
    } else {
        v_9.x = 1.0f;
        v_9.y = 0.0f;
        v_9.z = 1.0f;
        v_9.w = 1.0f;
    }
    v_13 = v_9;
    if (v.input_type == 3.0f) {
        let v_74 = v_13.xyz;
        let v_75 = (pow(abs((max((pow(abs(v_74), vec3<f32>(0.01268331333994865417f)) - vec3<f32>(0.8359375f)), vec3<f32>()) / (vec3<f32>(18.8515625f) - (pow(abs(v_74), vec3<f32>(0.01268331333994865417f)) * 18.6875f)))), vec3<f32>(6.27739477157592773438f)) * 10000.0f);
        let v_76 = v.sdr_white_point;
        let v_77 = (v_75 / vec3<f32>(v_76, v_76, v_76));
        v_13.x = v_77.x;
        v_13.y = v_77.y;
        v_13.z = v_77.z;
    }
    if !((v.tonemap_method == 0.0f)) {
        v_14 = v_13.xyz;
        if (v.tonemap_method == 1.0f) {
            let v_78 = v.tonemap_factor1;
            v_14 = (v_14 * v_78);
        } else if (v.tonemap_method == 2.0f) {
            if (v.input_type == 2.0f) {
                v_14 = (v_14 * mat3x3<f32>(vec3<f32>(0.62740397453308105469f, 0.32928299903869628906f, 0.04331300035119056702f), vec3<f32>(0.06909699738025665283f, 0.91954100131988525391f, 0.01136200036853551865f), vec3<f32>(0.01639099977910518646f, 0.08801300078630447388f, 0.89559501409530639648f)));
            }
            let v_79 = max(v_14.x, max(v_14.y, v_14.z));
            if (v_79 > 0.0f) {
                let v_80 = ((1.0f + (v.tonemap_factor1 * v_79)) / (1.0f + (v.tonemap_factor2 * v_79)));
                v_14 = (v_14 * v_80);
            }
            if (v.input_type == 2.0f) {
                v_14 = (v_14 * mat3x3<f32>(vec3<f32>(1.66049599647521972656f, -0.5876560211181640625f, -0.07283999770879745483f), vec3<f32>(-0.12454699724912643433f, 1.13289499282836914062f, -0.00834800023585557938f), vec3<f32>(-0.01815400086343288422f, -0.10059700161218643188f, 1.11875104904174804688f)));
            }
        }
        let v_81 = v_14;
        v_13.x = v_81.x;
        v_13.y = v_81.y;
        v_13.z = v_81.z;
    }
    if (v.input_type == 1.0f) {
        v_16 = v_13.xyz;
        if !((v.scRGB_output == 0.0f)) {
            let v_82 = v_16.x;
            var v_83: f32;
            if (v_82 <= 0.04044999927282333374f) {
                v_83 = (v_82 * 0.07739938050508499146f);
            } else {
                v_83 = pow((abs((v_82 + 0.05499999970197677612f)) * 0.94786733388900756836f), 2.40000009536743164062f);
            }
            v_16.x = v_83;
            let v_84 = v_16.y;
            var v_85: f32;
            if (v_84 <= 0.04044999927282333374f) {
                v_85 = (v_84 * 0.07739938050508499146f);
            } else {
                v_85 = pow((abs((v_84 + 0.05499999970197677612f)) * 0.94786733388900756836f), 2.40000009536743164062f);
            }
            v_16.y = v_85;
            let v_86 = v_16.z;
            var v_87: f32;
            if (v_86 <= 0.04044999927282333374f) {
                v_87 = (v_86 * 0.07739938050508499146f);
            } else {
                v_87 = pow((abs((v_86 + 0.05499999970197677612f)) * 0.94786733388900756836f), 2.40000009536743164062f);
            }
            v_16.z = v_87;
        }
        let v_88 = (v_16 * v.color_scale);
        v_15.x = v_88.x;
        v_15.y = v_88.y;
        v_15.z = v_88.z;
        v_15.w = v_13.w;
    } else if (v.input_type == 2.0f) {
        v_6 = (v_13.xyz * v.color_scale);
        if (v.scRGB_output == 0.0f) {
            let v_89 = v_6.x;
            var v_90: f32;
            if (v_89 <= 0.00313080009073019028f) {
                v_90 = (v_89 * 12.9200000762939453125f);
            } else {
                v_90 = ((pow(abs(v_89), 0.4166666567325592041f) * 1.05499994754791259766f) - 0.05499999970197677612f);
            }
            v_6.x = v_90;
            let v_91 = v_6.y;
            var v_92: f32;
            if (v_91 <= 0.00313080009073019028f) {
                v_92 = (v_91 * 12.9200000762939453125f);
            } else {
                v_92 = ((pow(abs(v_91), 0.4166666567325592041f) * 1.05499994754791259766f) - 0.05499999970197677612f);
            }
            v_6.y = v_92;
            let v_93 = v_6.z;
            var v_94: f32;
            if (v_93 <= 0.00313080009073019028f) {
                v_94 = (v_93 * 12.9200000762939453125f);
            } else {
                v_94 = ((pow(abs(v_93), 0.4166666567325592041f) * 1.05499994754791259766f) - 0.05499999970197677612f);
            }
            v_6.z = v_94;
            v_6 = clamp(v_6, vec3<f32>(), vec3<f32>(1.0f));
        }
        let v_95 = v_6;
        v_15.x = v_95.x;
        v_15.y = v_95.y;
        v_15.z = v_95.z;
        v_15.w = v_13.w;
    } else if (v.input_type == 3.0f) {
        let v_96 = (v_13.xyz * mat3x3<f32>(vec3<f32>(1.66049599647521972656f, -0.5876560211181640625f, -0.07283999770879745483f), vec3<f32>(-0.12454699724912643433f, 1.13289499282836914062f, -0.00834800023585557938f), vec3<f32>(-0.01815400086343288422f, -0.10059700161218643188f, 1.11875104904174804688f)));
        v_13.x = v_96.x;
        v_13.y = v_96.y;
        v_13.z = v_96.z;
        v_5 = (v_13.xyz * v.color_scale);
        if (v.scRGB_output == 0.0f) {
            let v_97 = v_5.x;
            var v_98: f32;
            if (v_97 <= 0.00313080009073019028f) {
                v_98 = (v_97 * 12.9200000762939453125f);
            } else {
                v_98 = ((pow(abs(v_97), 0.4166666567325592041f) * 1.05499994754791259766f) - 0.05499999970197677612f);
            }
            v_5.x = v_98;
            let v_99 = v_5.y;
            var v_100: f32;
            if (v_99 <= 0.00313080009073019028f) {
                v_100 = (v_99 * 12.9200000762939453125f);
            } else {
                v_100 = ((pow(abs(v_99), 0.4166666567325592041f) * 1.05499994754791259766f) - 0.05499999970197677612f);
            }
            v_5.y = v_100;
            let v_101 = v_5.z;
            var v_102: f32;
            if (v_101 <= 0.00313080009073019028f) {
                v_102 = (v_101 * 12.9200000762939453125f);
            } else {
                v_102 = ((pow(abs(v_101), 0.4166666567325592041f) * 1.05499994754791259766f) - 0.05499999970197677612f);
            }
            v_5.z = v_102;
            v_5 = clamp(v_5, vec3<f32>(), vec3<f32>(1.0f));
        }
        let v_103 = v_5;
        v_15.x = v_103.x;
        v_15.y = v_103.y;
        v_15.z = v_103.z;
        v_15.w = v_13.w;
    } else {
        let v_104 = v_13;
        v_17 = v_104;
        let v_105 = (v_104.xyz * v.color_scale);
        v_4.x = v_105.x;
        v_4.y = v_105.y;
        v_4.z = v_105.z;
        v_4.w = v_17.w;
        v_15 = v_4;
    }
    v_1 = (v_15 * v_2);
}

@fragment
fn main(@location(0u) v_106: vec4<f32>, @location(1u) v_107: vec2<f32>) -> @location(0u) vec4<f32> {
    main_inner(v_106, v_107);
    return v_1;
}
