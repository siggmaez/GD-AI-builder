"""Typed, local-only object properties. Never accept raw GD save strings."""
import math

TRIGGERS={899:"color",901:"move",1006:"pulse",1007:"alpha",1049:"toggle",1268:"spawn",1346:"rotate"}

def numeric(value,low,high):
    try:return type(value) in (int,float) and math.isfinite(value) and low<=value<=high
    except OverflowError:return False

def identity(value):
    return type(value) is int and 1<=value<=9999

def palette(value):
    if not isinstance(value,list) or len(value)>128: raise ValueError("Palette must have at most 128 colors")
    result=[]; seen=set()
    for color in value:
        if not isinstance(color,dict) or not identity(color.get("id")) or color["id"] in seen: raise ValueError("Invalid palette ID")
        if any(type(color.get(k)) is not int or not 0<=color[k]<=255 for k in ("r","g","b")): raise ValueError("Invalid RGB color")
        if type(color.get("blending",False)) is not bool or not numeric(color.get("opacity",1),0,1): raise ValueError("Invalid color opacity/blending")
        seen.add(color["id"])
        result.append({k:color[k] for k in ("id","r","g","b")} | {"blending":color.get("blending",False),"opacity":color.get("opacity",1)})
    return result

def properties(obj):
    out={}
    groups=obj.get("groups",[])
    if not isinstance(groups,list) or len(groups)>10 or any(not identity(g) for g in groups): raise ValueError("Invalid groups")
    if groups: out["groups"]=list(dict.fromkeys(groups))
    for key in ("base_color","detail_color"):
        if key in obj:
            if not identity(obj[key]): raise ValueError("Invalid color channel")
            out[key]=obj[key]
    for key,low,high in (("scale_x",.1,10),("scale_y",.1,10),("z_order",-1000,1000),("editor_layer",0,9999)):
        if key in obj:
            if not numeric(obj[key],low,high) or (key in ("z_order","editor_layer") and type(obj[key]) is not int): raise ValueError("Invalid "+key)
            out[key]=obj[key]
    if "z_layer" in obj:
        if type(obj["z_layer"]) is not int or obj["z_layer"] not in (-5,-3,-1,0,1,3,5,7,9,11): raise ValueError("Invalid Z layer")
        out["z_layer"]=obj["z_layer"]
    for key in ("flip_x","flip_y","no_glow"):
        if key in obj:
            if type(obj[key]) is not bool: raise ValueError("Invalid "+key)
            out[key]=obj[key]
    if obj["id"] in TRIGGERS:
        trigger=obj.get("trigger")
        if not isinstance(trigger,dict) or trigger.get("type")!=TRIGGERS[obj["id"]]: raise ValueError("Trigger needs matching typed parameters")
        kind=trigger["type"]
        allowed={"type","spawn_triggered","touch_triggered","multi_triggered"}|{
            "move":{"target_group","duration","move_x","move_y","easing"},
            "alpha":{"target_group","duration","opacity"},
            "toggle":{"target_group","activate"},"spawn":{"target_group","delay"},
            "rotate":{"target_group","center_group","duration","angle","easing"},
            "color":{"target_color","duration","r","g","b","opacity","blending"},
            "pulse":{"target_group","target_color","r","g","b","fade_in","hold","fade_out"}}[kind]
        if any(k not in allowed for k in trigger): raise ValueError("Unsupported trigger parameter")
        clean={"type":kind}
        for key in ("target_group","target_color","center_group"):
            if key in trigger:
                if not identity(trigger[key]): raise ValueError("Invalid trigger target")
                clean[key]=trigger[key]
        target="target_color" if kind=="color" else "target_group"
        if kind=="pulse": target="target_group" if "target_group" in clean else "target_color"
        if target not in clean: raise ValueError("Missing trigger target")
        if kind=="rotate" and "center_group" not in clean: raise ValueError("Rotate requires center group")
        for key,low,high in (("duration",0,120),("move_x",-30000,30000),("move_y",-30000,30000),("opacity",0,1),("easing",0,18),("angle",-3600,3600),("delay",0,120),("fade_in",0,120),("hold",0,120),("fade_out",0,120),("r",0,255),("g",0,255),("b",0,255)):
            if key in trigger:
                if not numeric(trigger[key],low,high): raise ValueError("Invalid trigger "+key)
                if key in ("r","g","b","easing") and type(trigger[key]) is not int: raise ValueError("Expected integer "+key)
                clean[key]=trigger[key]
        for key in ("activate","spawn_triggered","touch_triggered","multi_triggered","blending"):
            if key in trigger:
                if type(trigger[key]) is not bool: raise ValueError("Invalid trigger flag")
                clean[key]=trigger[key]
        if kind in ("color","pulse") and any(key not in clean for key in ("r","g","b")): raise ValueError("Color/Pulse requires RGB")
        out["trigger"]=clean
    elif "trigger" in obj: raise ValueError("Unsupported trigger object")
    return out

def validate_links(objects,colors):
    groups={g for obj in objects for g in obj.get("groups",[])}
    channels={c["id"] for c in colors}
    edges={}
    for obj in objects:
        for key in ("base_color","detail_color"):
            if key in obj and obj[key] not in channels: raise ValueError("Color channel missing from palette")
        t=obj.get("trigger",{})
        for key in ("target_group","center_group"):
            if key in t and t[key] not in groups: raise ValueError("Trigger targets an undefined local group")
        if "target_color" in t and t["target_color"] not in channels: raise ValueError("Trigger targets an undefined local color")
        if t.get("type")=="spawn":
            for group in obj.get("groups",[]): edges.setdefault(group,set()).add(t["target_group"])
    active=set();done=set()
    def visit(group):
        if group in active: raise ValueError("Cyclic Spawn chain")
        if group in done:return
        active.add(group)
        for target in edges.get(group,()):visit(target)
        active.remove(group);done.add(group)
    for group in edges:visit(group)
