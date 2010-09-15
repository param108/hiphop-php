/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010 Facebook, Inc. (http://www.facebook.com)          |
   | Copyright (c) 1997-2010 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#ifndef __EXT_IMAGESPRITE_H__
#define __EXT_IMAGESPRITE_H__

// >>>>>> Generated by idl.php. Do NOT modify. <<<<<<

#include <runtime/base/base_includes.h>
#include <gd.h>
#include <runtime/ext/ext_imagesprite_include.h>

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// class ImageSprite

FORWARD_DECLARE_CLASS(ImageSprite);
class c_ImageSprite : public ExtObjectData, public Sweepable {
 public:
  BEGIN_CLASS_MAP(ImageSprite)
  END_CLASS_MAP(ImageSprite)
  DECLARE_CLASS(ImageSprite, ImageSprite, ObjectData)
  DECLARE_INVOKES_FROM_EVAL
  ObjectData* dynCreate(CArrRef params, bool init = true);

  // need to implement
  public: c_ImageSprite();
  public: ~c_ImageSprite();
  public: void t___construct();
  public: Object t_addfile(CStrRef file, CArrRef options = null);
  public: Object t_addstring(CStrRef id, CStrRef data, CArrRef options = null);
  public: Object t_addurl(CStrRef url, int timeout_ms = 0, CArrRef Options = null);
  public: Object t_clear(CVarRef paths = null);
  public: Object t_loaddims(bool block = false);
  public: Object t_loadimages(bool block = false);
  public: String t_output(CStrRef output_file = null_string, CStrRef format = "png", int quality = 75);
  public: String t_css(CStrRef css_namespace, CStrRef sprite_file = null_string, CStrRef output_file = null_string, bool verbose = false);
  public: Array t_geterrors();
  public: Array t_mapping();
  public: Variant t___destruct();

  // implemented by HPHP
  public: c_ImageSprite *create();
  public: void dynConstruct(CArrRef Params);
  public: void dynConstructFromEval(Eval::VariableEnvironment &env,
                                    const Eval::FunctionCallExpression *call);
  public: virtual void destruct();

 private:
  void map();

 public:
  hphp_string_map<ImageSprite::ResourceGroup*> m_rsrc_groups;
  String m_image_string_buffer;
  bool m_current;
  hphp_string_map<ImageSprite::Image*> m_image_data;
  Array m_mapping;
  Array m_img_errors;
  Array m_sprite_errors;
  gdImagePtr m_image;
  int m_width;
  int m_height;
};

///////////////////////////////////////////////////////////////////////////////
}

#endif // __EXT_IMAGESPRITE_H__
