package com.puzzlingplans.gdx.vector;

import com.badlogic.gdx.graphics.Color;
import com.badlogic.gdx.math.Rectangle;
import com.badlogic.gdx.math.Vector2;
import com.badlogic.gdx.utils.Array;

public class VectorShape {

	public static class Vertex extends Vector2
	{
		public Vertex(float x, float y, boolean line, Color color, Float width) {
			super(x,y);
			this.line = line;
			this.color = color;
			this.width = width;
		}

		public boolean line;
		public Color color;
		public Float width;
	}
	
	//
	
	Array<Vertex> pts = new Array<Vertex>(false, 8, Vertex.class);
	Rectangle bounds;
	private static Vector2 tmp = new Vector2();

	public Color currentColor;
	public Float currentWidth;

	//
	
	public VectorShape() {
	}
	
	@Override
	public String toString() {
		return pts.size + " points, " + bounds;
	}
	
	private void newVertex(float x, float y, boolean line)
	{
		pts.add(new Vertex(x, y, line, currentColor, currentWidth));
	}

	public void moveTo(float x, float y) {
		newVertex(x, y, false);
	}

	public void lineTo(float x, float y) {
		newVertex(x, y, true);
	}

	public void finish() 
	{
		bounds = new Rectangle(pts.get(0).x, pts.get(0).y, 0, 0);
		for (int i=0; i<pts.size; i++)
		{
			bounds.merge(pts.get(i));
		}
		bounds.getCenter(tmp);
		float scale = 2.0f / Math.max(bounds.width, bounds.height);
		for (int i=0; i<pts.size; i++)
		{
			Vertex v = pts.get(i);
			v.x -= tmp.x;
			v.y -= tmp.y;
			v.x *= scale;
			v.y *= scale;
		}
		bounds.width *= scale;
		bounds.height *= scale;
	}

}
