
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

public class SVGVectorReader extends XmlReader {

	private Element root;
	private Map<String,VectorShape> shapes = new HashMap<String,VectorShape>();
	
	private Map<String,Color> text2color = new HashMap<String, Color>();
	
	//

	public SVGVectorReader(FileHandle file) 
	{
		super();
		try {
			root = parse(file);
		} catch (IOException e) {
			throw new GdxRuntimeException(e);
		}
		recurseGroups(root);
	}

	private void recurseGroups(Element el) {
		Array<Element> groups = el.getChildrenByName("g");
		for (int i=0; i<groups.size; i++)
		{
			Element group = groups.get(i);
			parseGroup(group);
		}
	}

	private void parseGroup(Element group) {
		String groupID = group.getAttribute("id");
		Array<Element> paths = group.getChildrenByName("path");
		if (paths.size > 0)
		{
			VectorShape shape = new VectorShape();
			for (int i=0; i<paths.size; i++)
			{
				Element path = paths.get(i);
				String d = path.getAttribute("d");
				// parse styles
				HashMap<String, String> styleAttrs = new HashMap<String,String>();
				String style = path.getAttribute("style");
				if (style != null)
				{
					String[] sarr = style.split(";");
					for (String s : sarr)
					{
						String[] nvarr = s.split(":", 2);
						if (nvarr.length == 2)
							styleAttrs.put(nvarr[0], nvarr[1]);
					}
				}
				// TODO: color, width
				shape['currentColor'] = textToColor(styleAttrs.get("stroke"), styleAttrs.get("stroke-opacity"));
				shape['currentWidth'] = textToFloat(styleAttrs.get("strokeWidth"));
				boolean hidden = "none".equals(styleAttrs.get("display"));
				String[] cmds = d.split(" ");
				float x = 0;
				float y = 0;
				int i0 = shape.pts.size;
				boolean start = true;
				for (int j=0; j<cmds.length; j++)
				{
					String cmd = cmds[j];
					char ch = cmd.charAt(0);
					switch (ch)
					{
					case 'm':
					case 'l':
						start = true;
						break;
					case 'z':
						Vertex origin = shape.pts.get(i0);
						shape.lineTo(origin.x, origin.y);
						break;
					case 'c': // cubic
						j++;
						break;
					default:
						String[] xy = cmd.split(",");
						if (xy.length != 2)
							throw new GdxRuntimeException("Could not parse " + groupID + ": " + cmd);
						x += Float.parseFloat(xy[0]);
						y -= Float.parseFloat(xy[1]);
						if (start || hidden)
							shape.moveTo(x,y);
						else
							shape.lineTo(x,y);
						start = false;
						break;
					}
				}
			}
			shape.finish();
			shapes.put(groupID, shape);
			System.out.println("Parsed " + groupID + " (" + shape + ")");
		}
		recurseGroups(group);
	}

	private Float textToFloat(String s) {
		if (s == null)
			return null;
		
		return Float.parseFloat(s);
	}

	private Color textToColor(String name, String opacity) {
		if (name == null || !name.startsWith("#"))
			return null;
		
		String key = name + ":" + opacity;
		Color color = text2color.get(key);
		if (color == null) {
			color = new Color(Integer.parseInt(name.substring(1), 16) << 8);
			if (opacity != null)
				color.a = Float.parseFloat(opacity);
			text2color.put(key, color);
			//System.out.println(key + " " + color);
		}
		return color;
	}

	public VectorShape getShape(String string) {
		return shapes.get(string);
	}
}
