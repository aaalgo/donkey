//Collapse>html
var collapseHeight='70';
$(".collapse").first().each(function() {
    collapseHeight=$(this).height();
})
$(".collapse").each(function () {
    var content=$(this).children(".html");
    if (collapseHeight<content.outerHeight(true)) {
        $(this).children(".mask").css("background"," linear-gradient(rgba(221,221,187,00), rgba(221,221,187,255))")
    }
    else{
        $(this).addClass("safe");
        $(this).children(".mask").hide();

    }
});
$(".collapse:not(.safe)").each(function () {
    var content=$(this).children(".html");
    $(this).click(function onExpand() {
        $(this).animate({height:content.outerHeight(true)});
        $(this).css("max-height",'999px');
        $(this).css("cursor","auto");
        $(this).children(".mask").hide();
        $(this).off('click',onExpand);

        $(this).dblclick(function onCollapse() {
            $(this).animate({height:collapseHeight});
            $(this).css("cursor","pointer");
            $(this).children(".mask").show();
            $(this).off('dblclick',onCollapse);
            $(this).click(onExpand);
        })
    })
});
$(".logout").click(function() {
    document.cookie = "user=; expires=Thu, 01 Jan 1970 00:00:00 UTC;path=/";
    location.reload(true);
})
var last_scroll='0px';
var scroll_direction='up';
$(window).scroll(function(){
    var cur_scroll=$(window).scrollTop();
    if (cur_scroll>last_scroll){
        //down
        if(scroll_direction!='down'){
            scroll_direction='down';
        $("nav").css("position","absolute");
        $("nav").css("top",cur_scroll);
        }

    }else {
        //up
        if(scroll_direction!='up'){
            scroll_direction='up';
        $("nav").css("position","fixed");
        $("nav").css("top",-$(this).outerHeight());
        $("nav").animate({top:0},700);

        }
    }
    last_scroll=cur_scroll;
})
