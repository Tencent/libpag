package org.libpag;

import android.content.res.AssetManager;
import android.util.Xml;

import org.extra.tools.LibraryLoadUtils;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.regex.Pattern;

public class PAGFont {
    public static PAGFont RegisterFont(String fontPath) {
        return RegisterFont(fontPath, 0);
    }

    public static PAGFont RegisterFont(AssetManager manager, String fileName) {
        return RegisterFont(manager, fileName, 0);
    }

    public static PAGFont RegisterFont(AssetManager manager, String fileName, int ttcIndex) {
        return RegisterFont(manager, fileName, ttcIndex, "", "");
    }
    public static native PAGFont RegisterFont(AssetManager manager, String fileName, int ttcIndex,
                                              String fontFamily, String fontStyle);

    public static PAGFont RegisterFont(String fontPath, int ttcIndex) {
        return RegisterFont(fontPath, ttcIndex, "", "");
    }
    public static native PAGFont RegisterFont(String fontPath, int ttcIndex,
                                              String fontFamily, String fontStyle);

    public static void UnregisterFont(PAGFont font) {
        UnregisterFont(font.fontFamily, font.fontStyle);
    }

    private static native void UnregisterFont(String fontFamily, String fontStyle);

    private static PAGFont RegisterFontBytes(byte[] bytes, int length, int ttcIndex) {
        return RegisterFontBytes(bytes, length, ttcIndex, "", "");
    }

    private static native PAGFont RegisterFontBytes(byte[] bytes, int length, int ttcIndex,
                                                    String fontFamily, String fontStyle);

    private static native void SetFallbackFontPaths(String[] fontNameList, int[] ttcIndices);

    public PAGFont() {
    }

    public PAGFont(String fontFamily, String fontStyle) {
        this.fontFamily = fontFamily;
        this.fontStyle = fontStyle;
    }

    /**
     * A string with the name of the font family.
     **/
    public String fontFamily = "";

    /**
     * A string with the style information — e.g., “bold”, “italic”.
     **/
    public String fontStyle = "";


    private static final String DefaultLanguage = "zh-Hans";
    private static final String SystemFontConfigPath_Lollipop = "/system/etc/fonts.xml";
    private static final String SystemFontConfigPath_JellyBean = "/system/etc/fallback_fonts.xml";
    private static final String SystemFontPath = "/system/fonts/";

    private static class FontConfig {
        String language = "";
        String fileName = "";
        int ttcIndex = 0;
        int weight = 400;
    }

    private static FontConfig[] parseLollipop()
            throws XmlPullParserException, IOException {
        FileInputStream fontsIn;
        FontConfig[] fontList = new FontConfig[0];
        try {
            fontsIn = new FileInputStream(SystemFontConfigPath_Lollipop);
        } catch (IOException e) {
            return fontList;
        }
        try {
            XmlPullParser parser = Xml.newPullParser();
            parser.setInput(fontsIn, null);
            parser.nextTag();
            fontList = readFamilies(parser);
        } finally {
            fontsIn.close();
        }
        return fontList;
    }

    private static FontConfig[] readFamilies(XmlPullParser parser)
            throws XmlPullParserException, IOException {
        ArrayList<FontConfig> fallbackList = new ArrayList<>();

        parser.require(XmlPullParser.START_TAG, null, "familyset");
        while (parser.next() != XmlPullParser.END_TAG) {
            if (parser.getEventType() != XmlPullParser.START_TAG) {
                continue;
            }
            String tag = parser.getName();
            if (tag.equals("family")) {
                readFamily(parser, fallbackList);
            } else {
                skip(parser);
            }
        }
        FontConfig[] list = new FontConfig[fallbackList.size()];
        fallbackList.toArray(list);
        return list;
    }

    private static void readFamily(XmlPullParser parser, ArrayList<FontConfig> fontList)
            throws XmlPullParserException, IOException {
        String name = parser.getAttributeValue(null, "name");
        String lang = parser.getAttributeValue(null, "lang");
        ArrayList<FontConfig> fonts = new ArrayList<>();
        while (parser.next() != XmlPullParser.END_TAG) {
            if (parser.getEventType() != XmlPullParser.START_TAG) {
                continue;
            }
            String tag = parser.getName();
            if (tag.equals("font")) {
                fonts.add(readFont(parser));
            } else {
                skip(parser);
            }
        }
        if (fonts.isEmpty()) {
            return;
        }
        FontConfig regularFont = null;
        for (FontConfig font : fonts) {
            if (font.weight == 400) {
                regularFont = font;
                break;
            }
        }
        if (regularFont == null) {
            regularFont = fonts.get(0);
        }
        if (!regularFont.fileName.isEmpty()) {
            regularFont.language = lang == null ? "" : lang;
            fontList.add(regularFont);
        }
    }

    private static final Pattern FILENAME_WHITESPACE_PATTERN =
            Pattern.compile("^[ \\n\\r\\t]+|[ \\n\\r\\t]+$");

    private static FontConfig readFont(XmlPullParser parser)
            throws XmlPullParserException, IOException {
        FontConfig font = new FontConfig();
        String indexStr = parser.getAttributeValue(null, "index");
        font.ttcIndex = indexStr == null ? 0 : Integer.parseInt(indexStr);
        String weightStr = parser.getAttributeValue(null, "weight");
        font.weight = weightStr == null ? 400 : Integer.parseInt(weightStr);
        StringBuilder filename = new StringBuilder();
        while (parser.next() != XmlPullParser.END_TAG) {
            if (parser.getEventType() == XmlPullParser.TEXT) {
                filename.append(parser.getText());
            }
            if (parser.getEventType() != XmlPullParser.START_TAG) {
                continue;
            }
            skip(parser);
        }

        font.fileName = SystemFontPath + FILENAME_WHITESPACE_PATTERN.matcher(filename).replaceAll("");
        return font;
    }

    private static void skip(XmlPullParser parser) throws XmlPullParserException, IOException {
        int depth = 1;
        while (depth > 0) {
            switch (parser.next()) {
                case XmlPullParser.START_TAG:
                    depth++;
                    break;
                case XmlPullParser.END_TAG:
                    depth--;
                    break;
            }
        }
    }

    private static FontConfig[] parseJellyBean() throws XmlPullParserException, IOException {
        FileInputStream fontsIn;
        FontConfig[] fontList = new FontConfig[0];
        try {
            fontsIn = new FileInputStream(SystemFontConfigPath_JellyBean);
        } catch (IOException e) {
            return fontList;
        }
        try {
            XmlPullParser parser = Xml.newPullParser();
            parser.setInput(fontsIn, null);
            parser.nextTag();
            fontList = readFamiliesJellyBean(parser);
        } finally {
            fontsIn.close();
        }
        return fontList;
    }

    private static FontConfig[] readFamiliesJellyBean(XmlPullParser parser)
            throws XmlPullParserException, IOException {
        ArrayList<FontConfig> fallbackList = new ArrayList<>();

        parser.require(XmlPullParser.START_TAG, null, "familyset");
        while (parser.next() != XmlPullParser.END_TAG) {
            if (parser.getEventType() != XmlPullParser.START_TAG) {
                continue;
            }
            String tag = parser.getName();
            if (tag.equals("family")) {
                while (parser.next() != XmlPullParser.END_TAG) {
                    if (parser.getEventType() != XmlPullParser.START_TAG) {
                        continue;
                    }
                    tag = parser.getName();
                    if (tag.equals("fileset")) {
                        readFileset(parser, fallbackList);
                    } else {
                        skip(parser);
                    }
                }
            } else {
                skip(parser);
            }
        }
        FontConfig[] list = new FontConfig[fallbackList.size()];
        fallbackList.toArray(list);
        return list;
    }

    private static void readFileset(XmlPullParser parser, ArrayList<FontConfig> fontList)
            throws XmlPullParserException, IOException {
        ArrayList<FontConfig> fonts = new ArrayList<>();
        while (parser.next() != XmlPullParser.END_TAG) {
            if (parser.getEventType() != XmlPullParser.START_TAG) {
                continue;
            }
            String tag = parser.getName();
            if (tag.equals("file")) {
                fonts.add(readFont(parser));
            } else {
                skip(parser);
            }
        }
        if (fonts.isEmpty()) {
            return;
        }
        FontConfig regularFont = null;
        for (FontConfig font : fonts) {
            if (font.weight == 400) {
                regularFont = font;
                break;
            }
        }
        if (regularFont == null) {
            regularFont = fonts.get(0);
        }
        if (!regularFont.fileName.isEmpty()) {
            fontList.add(regularFont);
        }
    }

    private static FontConfig getFontByLanguage(FontConfig[] fontList, String language) {
        language = language.toLowerCase();
        for (FontConfig fontConfig : fontList) {
            if (fontConfig.language.toLowerCase().equals(language)) {
                return fontConfig;
            }
        }
        return null;
    }

    private static void addFont(FontConfig fontConfig,
                                ArrayList<String> fontPaths,
                                ArrayList<Integer> ttcList) {
        if (fontPaths.contains(fontConfig.fileName)) {
            return;
        }
        File file = new File(fontConfig.fileName);
        if (!file.exists()) {
            return;
        }
        fontPaths.add(fontConfig.fileName);
        ttcList.add(fontConfig.ttcIndex);
    }

    private static boolean systemFontLoaded = false;

    private static void RegisterFallbackFonts() {
        if (systemFontLoaded) {
            return;
        }
        systemFontLoaded = true;
        FontConfig[] fontList = new FontConfig[0];
        File lollipopFile = new File(SystemFontConfigPath_Lollipop);
        if (lollipopFile.exists()) {
            try {
                fontList = parseLollipop();
            } catch (Exception e) {
                e.printStackTrace();
            }
        } else {
            try {
                fontList = parseJellyBean();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        ArrayList<String> fontPaths = new ArrayList<>();
        ArrayList<Integer> ttcList = new ArrayList<>();
        FontConfig font = getFontByLanguage(fontList, DefaultLanguage);
        if (font != null) {
            addFont(font, fontPaths, ttcList);
        }
        for (FontConfig fontConfig : fontList) {
            addFont(fontConfig, fontPaths, ttcList);
        }

        if (!fontPaths.isEmpty()) {
            String[] fontPathList = new String[fontPaths.size()];
            fontPaths.toArray(fontPathList);
            int[] ttcIndices = new int[ttcList.size()];
            int index = 0;
            for (Integer ttcIndex : ttcList) {
                ttcIndices[index++] = ttcIndex;
            }
            PAGFont.SetFallbackFontPaths(fontPathList, ttcIndices);
        }
    }

    static {
        LibraryLoadUtils.loadLibrary("pag");
    }
}
